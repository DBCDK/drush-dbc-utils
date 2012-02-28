# Drush DBC utils #

Utilities for Drush. Release build scripts.

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
fall back to the default branch otherwise.

	--dbc-modules=release

Checkout repositories from the newest release branch if it exists or
fall back to the default branch otherwise.

The release branches are branches named "release/.*" (the git-flow
standard) and the newest release branch is found be sorting the part
after "release/" with PHP's
[version\_compare()](http://php.net/manual/en/function.version-compare.php).
Version_compare will sort 7.x-1.2 before 7.x-1.3 etc. (although
usually that will not be a problem since git-flow deletes release
branches once they are finished).

	--dbc-modules=default
	
Checkout the repositories using their default branch. On GitHub the
default branch defaults to master - but you can configure the default
branch on a repository basis at GitHub.

Non-master defaults are useful if the master branch is actually used
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
