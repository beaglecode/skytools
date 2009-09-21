/*
 * logutriga.c - Smart trigger that logs urlencoded changes.
 *
 * Copyright (c) 2007 Marko Kreen, Skype Technologies OÜ
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <postgres.h>
#include <executor/spi.h>
#include <commands/trigger.h>
#include <lib/stringinfo.h>

#include "common.h"
#include "stringutil.h"

PG_FUNCTION_INFO_V1(pgq_logutriga);
Datum pgq_logutriga(PG_FUNCTION_ARGS);

void
pgq_urlenc_row(PgqTriggerEvent *ev, TriggerData *tg, HeapTuple row, StringInfo buf)
{
	TupleDesc	tupdesc = tg->tg_relation->rd_att;
	bool first = true;
	int i;
	const char *col_ident, *col_value;
	int attkind_idx = -1;

	for (i = 0; i < tg->tg_relation->rd_att->natts; i++)
	{
		/* Skip dropped columns */
		if (tupdesc->attrs[i]->attisdropped)
			continue;

		attkind_idx++;

		if (pgqtriga_skip_col(ev, tg, i, attkind_idx))
			continue;

		if (first)
			first = false;
		else
			appendStringInfoChar(buf, '&');

		/* quote column name */
		col_ident = SPI_fname(tupdesc, i + 1);
		pgq_encode_cstring(buf, col_ident, TBUF_QUOTE_URLENC);

		/* quote column value */
		col_value = SPI_getvalue(row, tupdesc, i + 1);
		if (col_value != NULL)
		{
			appendStringInfoChar(buf, '=');
			pgq_encode_cstring(buf, col_value, TBUF_QUOTE_URLENC);
		}
	}
}

/*
 * PgQ log trigger, takes 2 arguments:
 * 1. queue name to be inserted to.
 *
 * Queue events will be in format:
 *    ev_type   - operation type, I/U/D
 *    ev_data   - urlencoded column values
 *    ev_extra1 - table name
 *    ev_extra2 - optional urlencoded backup
 */
Datum
pgq_logutriga(PG_FUNCTION_ARGS)
{
	TriggerData *tg;
	struct PgqTriggerEvent	ev;
	HeapTuple	row;

	/*
	 * Get the trigger call context
	 */
	if (!CALLED_AS_TRIGGER(fcinfo))
		elog(ERROR, "pgq.logutriga not called as trigger");

	tg = (TriggerData *) (fcinfo->context);
	if (TRIGGER_FIRED_BY_UPDATE(tg->tg_event))
		row = tg->tg_newtuple;
	else
		row = tg->tg_trigtuple;

	if (pgq_is_logging_disabled())
		goto skip_it;

	/*
	 * Connect to the SPI manager
	 */
	if (SPI_connect() < 0)
		elog(ERROR, "logtriga: SPI_connect() failed");

	pgq_prepare_event(&ev, tg, true);

	appendStringInfoChar(ev.ev_type, ev.op_type);
	appendStringInfoChar(ev.ev_type, ':');
	appendStringInfoString(ev.ev_type, ev.pkey_list);
	appendStringInfoString(ev.ev_extra1, ev.info->table_name);

	/*
	 * create type, data
	 */
	pgq_urlenc_row(&ev, tg, row, ev.ev_data);

	/*
	 * Construct the parameter array and insert the log row.
	 */
	pgq_insert_tg_event(&ev);

	if (SPI_finish() < 0)
		elog(ERROR, "SPI_finish failed");

	/*
	 * After trigger ignores result,
	 * before trigger skips event if NULL.
	 */
skip_it:
	if (TRIGGER_FIRED_AFTER(tg->tg_event) || ev.skip)
		return PointerGetDatum(NULL);
	else
		return PointerGetDatum(row);
}

