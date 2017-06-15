#include <stdio.h>

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/tasks.h>

#define MAX_CMD	512

static char *datadir = "/Users/danielgustafsson/dev/postgresql/obj_10/data";
static char *bindir = "/Users/danielgustafsson/dev/postgresql/obj_10/bin";

int		page(struct http_request *);
int		pg_ctl_status(struct http_request *);
int		pg_ctl_start(struct http_request *);

static char * pg_ctl_cmd(char *command);
static int do_start(struct kore_task *t);

typedef struct rstate
{
	struct kore_task	task;
} rstate;

/*
 * Page handler for /. Do nothing but return 200 OK.
 */
int
page(struct http_request *req)
{
	http_response(req, 200, NULL, 0);
	return (KORE_RESULT_OK);
}

/*
 * Page handler for GET /start. Starts the PostgreSQL cluster in a detached
 * fork from the API process by using a kore background task.
 */
int
pg_ctl_start(struct http_request *req)
{
	rstate *state;

	/* Only allow GET for /start */
	if (req->method != HTTP_METHOD_GET)
	{
		http_response_header(req, "allow", "GET");
		http_response(req, HTTP_STATUS_METHOD_NOT_ALLOWED, NULL, 0);
		return KORE_RESULT_OK;
	}

	/*
	 * We only want one task setup, only create/run on the first invocation and
	 * not on the callback
	 */
	if (req->hdlr_extra == NULL)
	{
		state = kore_malloc(sizeof(rstate));
		req->hdlr_extra = state;

		kore_task_create(&state->task, do_start);
		kore_task_bind_request(&state->task, req);
		kore_task_run(&state->task);

		return KORE_RESULT_RETRY;
	}
	else
		state = req->hdlr_extra;

	if (kore_task_state(&state->task) != KORE_TASK_STATE_FINISHED)
	{
		http_request_sleep(req);
		return KORE_RESULT_RETRY;
	}

	if (kore_task_result(&state->task) != KORE_RESULT_OK)
	{
		http_response(req, HTTP_STATUS_INTERNAL_ERROR, "error", 5);
		return KORE_RESULT_OK;
	}

	http_response(req, HTTP_STATUS_OK, "ok", 2);
	return KORE_RESULT_OK;
}

/*
 * Start a postgresql server via `pg_ctl start` by means of fork/exec.
 *
 * TODO: - Drop the kore worker resources in the fork to avoid blocking a kore
 * 		   restart on holding the network bound.
 *		 - Report status back by doing a POST back so callback URL
 */
static int
do_start(struct kore_task *t)
{
	int		pid;
	char	cmd[1024];

	fflush(stdout);
	pid = fork();

	if (pid < 0)
		return KORE_RESULT_ERROR;

	if (pid > 0)
		return KORE_RESULT_OK;

	setsid();
	snprintf(cmd, sizeof(cmd), "exec \"%s/pg_ctl\" -D %s start", bindir, datadir);

	(void) execl("/bin/sh", "/bin/sh", "-c", cmd, (char *) NULL);

	exit(1);
	return KORE_RESULT_OK;
}

/*
 * Handler for GET /status. Executes `pg_ctl status` and captures the output
 * which is returned.
 *
 * TODO: - Wrap the return in a proper JSON envelope
 */
int
pg_ctl_status(struct http_request *req)
{
	char   *cmd_output;

	/* Only allow GET for /status */
	if (req->method != HTTP_METHOD_GET)
	{
		http_response_header(req, "allow", "GET");
		http_response(req, HTTP_STATUS_METHOD_NOT_ALLOWED, NULL, 0);
		return KORE_RESULT_OK;
	}

	cmd_output = pg_ctl_cmd("status");

	if (!cmd_output)
		http_response(req, HTTP_STATUS_INTERNAL_ERROR, NULL, 0);
	else
	{
		http_response(req, HTTP_STATUS_OK, cmd_output, strlen(cmd_output));
		kore_free(cmd_output);
	}
	return KORE_RESULT_OK;
}

/*
 * Execute, and capture the output of, a pg_ctl command. The returned value is
 * a kore_malloc'd string which must be freed by the caller.
 */
static char *
pg_ctl_cmd(char *command)
{
	char	cmd[MAX_CMD];
	char   *cmd_output;
	FILE   *output;

	snprintf(cmd, sizeof(cmd), "%s/pg_ctl -D \"%s\" %s", bindir, datadir, command);
	cmd_output = kore_malloc(1024);

	fflush(stdout);

	if ((output = popen(cmd, "r")) == NULL)
		return NULL;

	if ((fgets(cmd_output, 1024, output)) == NULL)
	{
		kore_free(cmd_output);
		cmd_output = NULL;
	}

	pclose(output);
	return cmd_output;
}
