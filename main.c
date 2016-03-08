#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <postgresql/libpq-fe.h>
#include <strings.h>

static void
exit_nicely(PGconn *conn)
{
    PQfinish(conn);
    exit(1);
}

int
main(int argc, char **argv)
{
    const char *conninfo;
    PGconn     *conn;
    PGresult   *res;
    PGnotify   *notify;

    PGresult   *int_res;
    char        int_query[60];
    char       *int_query_part = "select username, created from users where id="; // select required fields

    /*
     * If the user supplies a parameter on the command line, use it as the
     * conninfo string; otherwise default to setting dbname=postgres and using
     * environment variables or defaults for all other connection parameters.
     */
    if (argc > 1)
        conninfo = argv[1];
    else
        conninfo = "dbname = postgres";

    /* Make a connection to the database */
    conn = PQconnectdb(conninfo);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s",
                PQerrorMessage(conn));
        exit_nicely(conn);
    }

    /*
     * Issue LISTEN command to enable notifications from the rule's NOTIFY.
     */
    res = PQexec(conn, "LISTEN new_user");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "LISTEN command failed: %s", PQerrorMessage(conn));
        PQclear(res);
        exit_nicely(conn);
    }

    /*
     * should PQclear PGresult whenever it is no longer needed to avoid memory
     * leaks
     */
    PQclear(res);

    for (;;)
    {
        /*
         * Sleep until something happens on the connection.  We use select(2)
         * to wait for input, but you could also use poll() or similar
         * facilities.
         */
        int         sock;
        fd_set      input_mask;

        sock = PQsocket(conn);

        if (sock < 0)
            break;              /* shouldn't happen */

        FD_ZERO(&input_mask);
        FD_SET(sock, &input_mask);

        if (select(sock + 1, &input_mask, NULL, NULL, NULL) < 0)
        {
            fprintf(stderr, "select() failed: %s\n", strerror(errno));
            exit_nicely(conn);
        }

        /* Now check for input */
        PQconsumeInput(conn);
        while ((notify = PQnotifies(conn)) != NULL)
        {
            fprintf(stderr,
                    "ASYNC NOTIFY of '%s' received from backend PID %d payload %s\n",
                    notify->relname, notify->be_pid, notify->extra);
            PQfreemem(notify);

            // clear and prepare query
            strcpy(int_query,"");
            strcat(int_query,int_query_part);
            strcat(int_query,notify->extra);

            int_res=PQexec(conn,int_query);
            if (PQresultStatus(int_res) != PGRES_TUPLES_OK && PQntuples(int_res) == 1) 
            {
                fprintf(stderr, "SELECT command failed: %s", PQerrorMessage(conn));
                PQclear(int_res);
                exit_nicely(conn);
            }
            fprintf(stderr, "Added username %s at %s\n", PQgetvalue(int_res,0,0),PQgetvalue(int_res,0,1));
            
            /* perform external action here */

            PQclear(int_res);
        }
    }

    /* close the connection to the database and cleanup */
    PQfinish(conn);

    return 0;
}
