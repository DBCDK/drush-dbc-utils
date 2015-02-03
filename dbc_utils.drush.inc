<?php
/**
 * @file
 * Utilities for Drush. Release build scripts.
 *
 * © 2012 DBC A/S and TING.Community
 *
 * Version: 1.2.1.
 */

/**
 * Implements hook_drush_help_alter().
 *
 * In Drush 5 we must define the options.
 */
function dbc_utils_drush_help_alter(&$command) {
  if ($command['command'] == 'make' || $command['command'] == 'ding-deploy') {
    $command['options']['dbc-modules'] = 'Which branches to checkout';
    $command['options']['dbc-github-org'] = 'Which GitHub organization to manipulate branches on.';
  }
}

/**
 * Implements hook_make_validate_info().
 *
 * This is the Drush 5 variant of the validate makefile hook.
 */
function dbc_utils_make_validate_info($info) {
  return _dbc_utils_make_validate_info($info);
}

/**
 * Internal version of the validate makefile.
 */
function _dbc_utils_make_validate_info($info) {
  $option = drush_get_option('dbc-modules', FALSE);
  $valid_option_values = array('HEAD', 'develop', 'release');

  if (!$option) {
    return $info;
  }

  $repos = drush_get_option('dbc-github-org', 'DBCDK');
  if (drush_get_option('ding2-dev', FALSE)) {
    return drush_set_error('DBC_UTILS_ERROR', dt('--ding2-dev is not compatible with --dbc-modules.'));
  }
  drush_log(dt("Checking out $option branches of $repos Github organization repositories."), 'ok');


  try {
    $modules = getGitHeadsFromBranches($info, $repos);
  } catch (Exception $e) {
    return drush_set_error('DBC_UTILS_ERROR', $e->getMessage());
  }

  foreach (array('projects', 'libraries',) as $key) {
    if (isset($info[$key])) {
      foreach ($info[$key] as $name => &$spec) {
        // Only consider git repositories.
        if (!empty($spec['download']['type']) && $spec['download']['type'] == 'git') {
          // Only mess with repos from the right organisations on
          // github. The [/:] makes this work with both http and git
          // protocol specifications.
          if (preg_match("{github\.com[/:]($repos)/}i", $spec['download']['url']) || preg_match("{git\.dbc\.dk}i", $spec['download']['url']) ) {
            // If we want the master branch for modules select it (actually unset
            // branches and let GitHub what the master branch is.
            if ($option == 'HEAD') {
              if (isset($spec['download']['branch'])) {
                unset($spec['download']['branch']);
              }
            }
            elseif ($option == 'develop') {
              // If we want the develop branch for modules select it if it exists -
              // otherwise select the HEAD branch.
              // Expect a develop branch to exist in all modules.

              $spec['download']['branch'] = 'develop';
            }
            elseif ($option == 'release') {
              // If we want a release branch for modules select the most recent
              // release branch (based on the version number) - if no release
              // branches exists select the HEAD branch.


              $branches = array();
              $releases = array();

              // Locate branches and release branches (git flow) on the repository.
              //drush_shell_exec('git ls-remote --heads %s', $spec['download']['url']);
              $modulename = end(explode("/", $spec['download']['url']));

              foreach ($modules[$modulename] as $branch) {
                $branches[] = preg_replace('@.*refs/heads/(.*)@', '$1', $branch);
                if (preg_match('@.*refs/heads/release/(.*)@', $branch, $matches)) {
                  $releases = $matches[0];
                }
              }

              if (empty($releases)) {
                if (isset($spec['download']['branch'])) {
                  unset($spec['download']['branch']);
                }
              }
              else {
                // @TODO this sort is broken for a rare special case of version
                // number comparison: '1.2' and '1.2+dbc1'.
                uasort($releases, 'version_compare');
                $spec['download']['branch'] = 'release/' . array_pop($releases);
              }
            }
            else {
              // feature branch given.. if name exists in module check it out.
              // else check out develop
              $module_array = explode("/", $spec['download']['url']);
              $modulename = end($module_array);
              if (isset($modules[$modulename]) && hasGitBranch($modules[$modulename], $option)) {
                $spec['download']['branch'] = $option;
              }
              else {
                $spec['download']['branch'] = 'develop';
              }
            }

            drush_log(dt('Using @branch for @repos.',
              array(
                '@branch' => $spec['download']['branch'] ? $spec['download']['branch'] : 'the HEAD branch',
                '@repos' => $spec['download']['url'] ? $spec['download']['url']: $modulename,
              )), 'ok');

            // Unset any tag or revision specifications.
            if (isset($spec['download']['tag'])) {
              unset($spec['download']['tag']);
            }
            if (isset($spec['download']['revision'])) {
              unset($spec['download']['revision']);
            }
          }
        }
      }
    }
  }

  return $info;
}

/**
 * @param $lines lines from git ls-remote --heads
 * @param $branch branch name to look for
 * @return bool true if branch is located in the lines
 */
function hasGitBranch($lines, $branch) {
  foreach ($lines as $line) {
    if (strpos($line, $branch) !== FALSE ) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * fetch Git Heads from branches in baralell.
 * @param $info
 * @param $repos // repos option from
 * @throws Exception
 * @return mappe with 'modulename' as key and lines from each git ls-remote ...
 */
function getGitHeadsFromBranches($info, $repos) {
  $modules_to_check = "";
  foreach (array('projects', 'libraries',) as $key) {
    if (isset($info[$key])) {
      foreach ($info[$key] as $name => $spec) {
        if (empty($spec['download']['type'])) {
          continue;
        }
        if ($spec['download']['type'] != 'git') {
          continue;
        }
        if (!preg_match("{github\.com[/:]($repos)/}i", $spec['download']['url']) && !preg_match("{git\.dbc\.dk}i", $spec['download']['url'])) {
          continue;
        }

        $modules_to_check = $modules_to_check . ' '  . $spec['download']['url'];
      }
    }
  }

  if($modules_to_check == "") {
        return;
  }

  //! ok to run with concurrency 10 for checks when concurrency is not set by user
  $concurrency = drush_get_option('concurrency', '10');

  // some distros force parallel to run in --trollef mode using --gnu to overwrite.
  $cmd = "parallel --gnu -j ".$concurrency." git ls-remote --heads {} \| sed -e 's/^/{/}+/' ::: ".$modules_to_check ;
  $res=drush_shell_exec( $cmd );

  if( ! $res ) {
    drush_log(dt("check for branched failed command used : ".$cmd.""), 'error');
    throw new Exception("Unable to look up git heads");
  }

  $modules = array();
  foreach (drush_shell_exec_output() as $line) {
    $split_line = explode("+", $line);
    $modules[$split_line[0]][] = $split_line[1];
  }

  return $modules;
}
