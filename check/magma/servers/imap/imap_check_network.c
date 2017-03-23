
/**
 * @file /magma/check/magma/servers/imap/imap_check_helpers.c
 *
 * @brief Functions used to test IMAP connections over a network connection.
 *
 */

#include "magma_check.h"

/**
 * @brief 	Calls client_read_line on a client until it finds a line matching "<tag> OK".
 *
 * @param client 	The client to read from (which should be connected to an IMAP server).
 * @param token 	The unique token that identifies the current imap command dialogue.
 *
 * @return 	Returns true if client_read_line was successful until the last line was found.
 * 			specified in num and there was no error. Otherwise returns false.
 */
bool_t check_imap_client_read_end(client_t *client, chr_t *tag) {

	bool_t outcome = false;
	stringer_t *last_line = st_merge("ss", NULLER(tag), NULLER(" OK"));

	while (!outcome && client_read_line(client) > 0) {
		if (!st_cmp_cs_starts(&client->line, last_line)) outcome = true;
	}

	st_cleanup(last_line);
	return outcome;
}

/**
 * @brief 	Prints the LOGIN command to the passed client using the passed credentials.
 *
 * @param	client	The client_t* to print the commands to. It should be connected to an IMAP server.
 * @param	user	A chr_t* holding the username to use in the LOGIN command.
 * @param	pass	A chr_t* holding the password to use in the LOGIN command.
 * @param	tag		A chr_t* holding the tag to place at the beginning of the LOGIN command.
 * @param	errmsg	A stringer_t* into which the error message will be printed in the even of an error.
 * @return	True if the LOGIN command was successful, otherwise false.
 */
bool_t check_imap_client_login(client_t *client, chr_t *user, chr_t *pass, chr_t *tag, stringer_t *errmsg) {

	stringer_t *login_line;
	uint32_t login_line_len = ns_length_get(tag) + ns_length_get(user) + ns_length_get(pass) + 10;

	// Construct the login command
	if (!(login_line = st_merge("nsnsns", tag, NULLER(" LOGIN "), user, NULLER(' '), pass, NULLER("\r\n")))) {

		st_sprint(errmsg, "Failed to construct the login command.");
		return false;
	}
	// Test the LOGIN command.
	else if (client_print(client, st_char_get(login_line)) != login_line_len || !check_imap_client_read_end(client, tag) ||
			client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER(tag))) {

		st_sprint(errmsg, "Failed to return a successful state after LOGIN.");
		return false;
	}

	st_cleanup(login_line);
	return true;
}

/**
 * @brief	Prints the SELECT command to the passed client using the passed parameter.
 *
 * @param	client	The client_t* to print the command to. It should be connected to an IMAP server.
 * @param	folder	A chr_t* holding the name of the folder to select.
 * @param	tag		A chr_t* holding the tag to place at the beginning of the SELECT command.
 * @param	errmsg	A stringer_t* into which the error message will be printed in the even of an error.
 * @return	True if the SELECT command was successful, otherwise false.
 */
bool_t check_imap_client_select(client_t *client, chr_t *folder, chr_t *tag, stringer_t *errmsg) {

	// Test the SELECT command.
	if (client_print(client, "A2 SELECT Inbox\r\n") <= 0 || !check_imap_client_read_end(client, tag) ||
		client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER(tag))) {

		st_sprint(errmsg, "Failed to return a successful state after SELECT.");
		return false;
	}

	return true;
}

bool_t check_imap_client_close_logout(client_t *client, stringer_t *errmsg) {

	// Test the CLOSE command.
	if (client_print(client, "A4 CLOSE\r\n") <= 0 || !check_imap_client_read_end(client, "A4") ||
			client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("A4 OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after CLOSE.");
		return false;
	}

	// Test the LOGOUT command.
	else if (client_print(client, "A5 LOGOUT\r\n") <= 0 || !check_imap_client_read_end(client, "A5") ||
			client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("A5 OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after LOGOUT.");
		return false;
	}

	return true;
}

bool_t check_imap_network_basic_sthread(stringer_t *errmsg, uint32_t port, bool_t secure) {

	client_t *client = NULL;

	// Check the initial response.
	if (!(client = client_connect("localhost", port)) || (secure && (client_secure(client) == -1)) ||
		!net_set_timeout(client->sockd, 20, 20) || client_read_line(client) <= 0 || (client->status != 1) ||
		st_cmp_cs_starts(&(client->line), NULLER("* OK"))) {

		st_sprint(errmsg, "Failed to connect with the IMAP server.");
		client_close(client);
		return false;
	}

	// Test the LOGIN command.
	else if (client_print(client, "A1 LOGIN princess password\r\n") <= 0 || !check_imap_client_read_end(client, "A1") ||
			client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("A1 OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after LOGIN.");
		client_close(client);
		return false;
	}

	// Test the SELECT command.
	else if (client_print(client, "A2 SELECT Inbox\r\n") <= 0 || !check_imap_client_read_end(client, "A2") ||
			client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("A2 OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after SELECT.");
		client_close(client);
		return false;
	}

	// Test the FETCH command.
	else if (client_print(client, "A3 FETCH 1 RFC822\r\n") <= 0 || !check_imap_client_read_end(client, "A3") ||
			client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("A3 OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after FETCH.");
		client_close(client);
		return false;
	}

	// Test the CLOSE command.
	else if (client_print(client, "A4 CLOSE\r\n") <= 0 || !check_imap_client_read_end(client, "A4") ||
			client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("A4 OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after CLOSE.");
		client_close(client);
		return false;
	}

	// Test the LOGOUT command.
	else if (client_print(client, "A5 LOGOUT\r\n") <= 0 || !check_imap_client_read_end(client, "A5") ||
			client_status(client) != 1 || st_cmp_cs_starts(&(client->line), NULLER("A5 OK"))) {

		st_sprint(errmsg, "Failed to return a successful state after LOGOUT.");
		client_close(client);
		return false;
	}

	client_close(client);

	return true;
}


bool_t check_imap_network_search_sthread(stringer_t *errmsg, uint32_t port) {

	client_t *client = NULL;

	// Check the initial response.
	if (!(client = client_connect("localhost", port)) || (secure && (client_secure(client) == -1)) ||
		!net_set_timeout(client->sockd, 20, 20) || client_read_line(client) <= 0 || (client->status != 1) ||
		st_cmp_cs_starts(&(client->line), NULLER("* OK"))) {

		st_sprint(errmsg, "Failed to connect with the IMAP server.");
		client_close(client);
		return false;
	}
	// Test the LOGIN command.
	else if (!check_imap_client_login(client, "princess", "password", errmsg)) {
		return false;
	}

	return true;
}

bool_t check_imap_network_fetch_sthread(stringer_t *errmsg, uint32_t port) {

	client_t *client = NULL;

	// Check the initial response.
	if (!(client = client_connect("localhost", port)) || (secure && (client_secure(client) == -1)) ||
		!net_set_timeout(client->sockd, 20, 20) || client_read_line(client) <= 0 || (client->status != 1) ||
		st_cmp_cs_starts(&(client->line), NULLER("* OK"))) {

		st_sprint(errmsg, "Failed to connect with the IMAP server.");
		client_close(client);
		return false;
	}
	// Test the LOGIN command.
	else if (!check_imap_client_login(client, "princess", "password", errmsg)) {
		return false;
	}

	return true;
}
