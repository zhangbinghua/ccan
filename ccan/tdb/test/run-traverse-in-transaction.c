#define _XOPEN_SOURCE 500
#include "lock-tracking.h"
#define fcntl fcntl_with_lockcheck
#include <ccan/tdb/tdb.h>
#include <ccan/tdb/io.c>
#include <ccan/tdb/tdb.c>
#include <ccan/tdb/lock.c>
#include <ccan/tdb/freelist.c>
#include <ccan/tdb/traverse.c>
#include <ccan/tdb/transaction.c>
#include <ccan/tdb/error.c>
#include <ccan/tdb/open.c>
#include <ccan/tdb/check.c>
#include <ccan/tdb/hash.c>
#include <ccan/tap/tap.h>
#undef fcntl_with_lockcheck
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include "external-agent.h"
#include "logging.h"

static struct agent *agent;

static bool correct_key(TDB_DATA key)
{
	return key.dsize == strlen("hi")
		&& memcmp(key.dptr, "hi", key.dsize) == 0;
}

static bool correct_data(TDB_DATA data)
{
	return data.dsize == strlen("world")
		&& memcmp(data.dptr, "world", data.dsize) == 0;
}

static int traverse(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data,
		     void *p)
{
	ok1(correct_key(key));
	ok1(correct_data(data));
	return 0;
}

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	TDB_DATA key, data;

	plan_tests(13);
	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	tdb = tdb_open_ex("run-traverse-in-transaction.tdb",
			  1024, TDB_CLEAR_IF_FIRST, O_CREAT|O_TRUNC|O_RDWR,
			  0600, &taplogctx, NULL);
	ok1(tdb);

	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dptr = (void *)"world";
	data.dsize = strlen("world");

	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);

	ok1(external_agent_operation(agent, OPEN, tdb_name(tdb)) == SUCCESS);

	ok1(tdb_transaction_start(tdb) == 0);
	ok1(external_agent_operation(agent, TRANSACTION_START, tdb_name(tdb))
	    == WOULD_HAVE_BLOCKED);
	tdb_traverse(tdb, traverse, NULL);

	/* That should *not* release the transaction lock! */
	ok1(external_agent_operation(agent, TRANSACTION_START, tdb_name(tdb))
	    == WOULD_HAVE_BLOCKED);
	tdb_traverse_read(tdb, traverse, NULL);

	/* That should *not* release the transaction lock! */
	ok1(external_agent_operation(agent, TRANSACTION_START, tdb_name(tdb))
	    == WOULD_HAVE_BLOCKED);
	ok1(tdb_transaction_commit(tdb) == 0);
	/* Now we should be fine. */
	ok1(external_agent_operation(agent, TRANSACTION_START, tdb_name(tdb))
	    == SUCCESS);

	tdb_close(tdb);

	return exit_status();
}
