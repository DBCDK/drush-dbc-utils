# Drush DBC utils #

Utilities for Drush. Release build scripts.

Version 1.1.0.

## Use case ##

Drush make is wonderful for building sites.

However during development and release test you would often want to
use the bleeding edge version of Git repositories whereas you want to
specify a specific tag for release purposes.

Changing the make file all the time or having trying to keep several
make files in sync is doomed to fail.

Enter:

    drush make --dbc-modules=develop ...

This will checkout the repositories with their develop branches.

## Usage ##

The options and their possible values are documented below:

	--dbc-modules=develop

Checkout repositories from a branch named "develop" if it exists or
fall back to the HEAD branch otherwise.

	--dbc-modules=release

Checkout repositories from the newest release branch if it exists or
fall back to the HEAD branch otherwise.

The release branches are branches named "release/.*" (the git-flow
standard) and the newest release branch is found be sorting the part
after "release/" with PHP's
[version\_compare()](http://php.net/manual/en/function.version-compare.php).
Version_compare will sort 7.x-1.2 before 7.x-1.3 etc. (although
usually that will not be a problem since git-flow deletes release
branches once they are finished).

	--dbc-modules=HEAD
	
Checkout the repositories using their HEAD branch. On GitHub the HEAD
branch defaults to master - but you can configure the HEAD branch on a
repository basis at GitHub.

Non-master HEADs are useful if the master branch is actually used
for tracking an upstream/forked repository.

	--dbc-github-org=<org-re> (default: DBCDK)
	
The ``--dbc-modules`` option will only affect GitHub organizations
matching the ``<org-re>`` regular expression (case insensitive).

It defaults to "DBCDK", but if you want it to operate on i.e. both the
DBCDK and ding2 organizations you could specify
``--dbc-github-org='ding2|dbcdk'``.

	--ding2-dev
	
This is not an option of Drush DBC utils, but an option from [Drush
Ding2 utils](https://github.com/ding2/drush-ding2-utils). The option
is incompatible with ``--dbc-modules`` and trying to use both at the
same time will have drush make fail.
	
## Compatibility ##

* [Drush](http://drupal.org/project/drush) version 4.5 with
  * [Drush_make](http://drupal.org/project/drush_make) version 2.3
* [Drush](http://drupal.org/project/drush) version 5

The usage of branches is designed to work together with
[git-flow](https://github.com/nvie/gitflow).

## When to use what? ##

### A nightly test build ###

A nightly test build should use the make file from its develop branch
combine with ``--dbc-modules=develop`` to track the most recent
development.

This will typically be the version used for testing user stories
during a sprint.

### Making integration test of the "most likely next" version ###

Making integration test of the "most likely next" version (the "most
likely next" version is when you haven't decided on a release yet and
therefor haven't made a release branch) should use the master/HEAD
branch of the make file combined with ``--dbc-modules=HEAD``.

This will typically be used for sprint demo and ongoing integration
testing.

### Release preparation ###

Preparing for a release would mean creating a release branch on the
make file and using that combined with tagged version of repositories
mentioned in the make file. Hence no need for ``--dbc-modules``.

Under rare circumstances you might want to use
``--dbc-modules=release``.

### Creating a release ###

Creating a release would mean using a tagged version of the make file
and combining it with tagged version of repositories mentioned in the
make file. Hence no need for ``--dbc-modules``.
