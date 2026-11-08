#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Errors are ignored if only possible error for given function is OOM.

// IMPORTANT! Theoretically the Response signal may arrive before we register
// match rule and install our filter, we only do this in this order for
// simplicity, which is fine for file opener since the user interaction (even if
// cancelled immediately) takes forever relatively speaking, so the race
// condition will not be hit in practice. That being said, some other portals
// have been observed to emit Response immediately, so this code won't work with
// other portals. What we should do is pass a handle_token to the options dict,
// so that we can predict the request object path.

// cleanup attribute destructor
void connection_unref(DBusConnection** connection)
{
    dbus_connection_unref(*connection);
}

// cleanup attribute destructor
void message_unref(DBusMessage** message)
{
    dbus_message_unref(*message);
}

#define fail(error) \
    return !!fprintf(stderr, "DBUS ERROR (line %i): %s: %s.\n", __LINE__, (error).name, (error).message)

DBusHandlerResult filter(DBusConnection* connection, DBusMessage* message, void* out_response)
{
    (void)connection;

    if ( ! dbus_message_is_signal(message, "org.freedesktop.portal.Request", "Response"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_message_ref(message); // message escaping, remember to unref.
    DBusMessage** response = out_response;
    *response = message;
    return DBUS_HANDLER_RESULT_HANDLED;
}

int main(void)
{
    DBusError error = DBUS_ERROR_INIT;

    // Connect to D-Bus.
    __attribute__((cleanup(connection_unref)))
    DBusConnection* connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (connection == NULL)
        fail(error);

    // Construct remote procedure call.
    __attribute__((cleanup(message_unref)))
    DBusMessage* message = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop", // bus name
        "/org/freedesktop/portal/desktop", // path
        "org.freedesktop.portal.FileChooser", // interface
        "OpenFile"); // method

    DBusMessageIter iter;
    DBusMessageIter iter_dict;
    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_append_basic(&iter, 's', &(char*){""}); // parent window ID (optional)
    dbus_message_iter_append_basic(&iter, 's', &(char*){"Hey mr. Prinssi, let's open a file!"}); // title
    dbus_message_iter_open_container(&iter, 'a', "{sv}", &iter_dict); { // options
        // Note: open_container() appends "uninitialized" value, we construct it here.
        // Also note: having any entries in the options dictionary is optional.
        #if 1 // add {"accept_label", "Pick me!"} entry to options.
        DBusMessageIter iter_entry;
        dbus_message_iter_open_container(&iter_dict, 'e', NULL, &iter_entry); {
            DBusMessageIter iter_variant;
            dbus_message_iter_append_basic(&iter_entry, 's', &(char*){"accept_label"});
            dbus_message_iter_open_container(&iter_entry, 'v', "s", &iter_variant); {
                dbus_message_iter_append_basic(&iter_variant, 's', &(char*){"Pick me!"});
            } dbus_message_iter_close_container(&iter_entry, &iter_variant);
        } dbus_message_iter_close_container(&iter_dict, &iter_entry);
        #endif
    } dbus_message_iter_close_container(&iter, &iter_dict);

    // Invoke remote procedure call and wait for response. File opening dialog
    // opens.
    __attribute__((cleanup(message_unref)))
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        connection, message, DBUS_TIMEOUT_USE_DEFAULT, &error);
    if (reply == NULL)
        fail(error);

    // Parse response.
    const char* object_path;
    if ( ! dbus_message_get_args(reply, &error, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID))
        fail(error);

    // Set rule to match the expected response message and callback (filter) to
    // handle the message.
    __attribute__((cleanup(message_unref))) DBusMessage* response = NULL;
    char rule[DBUS_MAXIMUM_MATCH_RULE_LENGTH] = "type='signal',path=";
    strcat(rule, object_path);
    dbus_bus_add_match(connection, rule, &error);
    if (dbus_error_is_set(&error))
        fail(error);
    dbus_connection_add_filter(connection, filter, &response, NULL);

    // Wait for response message in event loop. We don't actually need an event
    // loop, but this demonstrates common code.
    while (true) {
        // Block until events available.
        if ( ! dbus_connection_read_write(connection, -1)) {
            puts("Connection closed.");
            break;
        }
        while (dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS)
            ;

        // Again, we don't need an event loop in this case.
        break;
    }

    // Validate response message.
    DBusMessageIter iter_response;
    if ( ! response)
        return !!fprintf(stderr, "Response is NULL somehow.\n");
    if ( ! dbus_message_iter_init(response, &iter_response))
        return !!fprintf(stderr, "Response has no arguments for some reason.\n");
    if (strcmp(dbus_message_get_signature(response), "ua{sv}") != 0)
        return !!fprintf(stderr, "Unexpected response signature: %s\n", dbus_message_get_signature(response));

    // Read response message.
    uint32_t status;
    dbus_message_iter_get_basic(&iter_response, &status);
    dbus_message_iter_next(&iter_response);
    switch (status) {
    case 1: puts("Cancelled."); return EXIT_SUCCESS;
    case 2: puts("Got cancelled by someone."); return EXIT_SUCCESS;
    }
    dbus_message_iter_recurse(&iter_response, &iter_dict);
    while (dbus_message_iter_get_arg_type(&iter_dict) != DBUS_TYPE_INVALID) {
        DBusMessageIter iter_entry;
        DBusMessageIter iter_value; // variant
        dbus_message_iter_recurse(&iter_dict, &iter_entry);
        dbus_message_iter_next(&iter_dict);
        const char* key;
        dbus_message_iter_get_basic(&iter_entry, &key);
        dbus_message_iter_next(&iter_entry);
        dbus_message_iter_recurse(&iter_entry, &iter_value);

        if (strcmp(key, "uris") == 0) {
            DBusMessageIter iter_uris;
            dbus_message_iter_recurse(&iter_value, &iter_uris);
            while (dbus_message_iter_get_arg_type(&iter_uris) != DBUS_TYPE_INVALID) {
                const char* uri;
                dbus_message_iter_get_basic(&iter_uris, &uri);
                dbus_message_iter_next(&iter_uris);

                puts(uri);
            }
        } // else if (strcmp(key, other keys here if needed) == 0)
    }

    // NOTE: we should _not_ close connection using dbus_connection_close(), the
    // connection may be shared. We should unref instead, which we do at cleanup.
    return EXIT_SUCCESS;
}
