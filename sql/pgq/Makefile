
EXTENSION = pgq

EXT_VERSION = 3.1.1
EXT_OLD_VERSIONS = 3.1

DOCS = README.pgq

Contrib_data = structure/uninstall_pgq.sql

Contrib_regress   = pgq_init_noext pgq_core pgq_perms logutriga sqltriga $(trunc_test)
Extension_regress = pgq_init_ext   pgq_core pgq_perms logutriga sqltriga $(trunc_test)

# MAJORVERSION was defined in 8.4
trunc_test = $(if $(MAJORVERSION),trunctrg)

include ../common-pgxs.mk

SUBDIRS = lowlevel triggers

# PGXS does not have subdir support, thus hack to recurse into lowlevel/
all: sub-all
install: sub-install
clean: sub-clean
distclean: sub-distclean
sub-all sub-install sub-clean sub-distclean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $(subst sub-,,$@) \
			DESTDIR=$(DESTDIR) \
			PG_CONFIG=$(PG_CONFIG) \
		|| exit 1; \
	done

lowlevel/pgq_lowlevel.sql: sub-all
triggers/pgq_triggers.sql: sub-all

#
# docs
#
dox: cleandox $(SRCS)
	mkdir -p docs/html
	mkdir -p docs/sql
	$(CATSQL) --ndoc structure/tables.sql > docs/sql/schema.sql
	$(CATSQL) --ndoc structure/func_public.sql > docs/sql/external.sql
	$(CATSQL) --ndoc structure/func_internal.sql > docs/sql/internal.sql
	$(CATSQL) --ndoc structure/triggers.sql > docs/sql/triggers.sql
	$(NDOC) $(NDOCARGS)

