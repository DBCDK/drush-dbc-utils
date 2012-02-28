<?php
/**
 * @file
 * Utilities for Drush. Release build scripts.
 *
 * © 2012 DBC A/S and TING.Community
 *
 * Version: 1.0.0.
 */

/**
 * Implements hook_drush_help_alter().
 *
 * In Drush 5 we must define the options.
 */
function dbc_utils_drush_help_alter(&$command) {
  if ($command['command'] == 'make') {
    $command['options']['dbc-modules'] = 'Which branches to checkout';
    $command['options']['dbc-github-org'] = 'Which GitHub organization to manipulate branches on.';
  }
}

/**
 * Implements hook_drush_make_validate_info().
 *
 * This is the Drush 4.5 variant of the validate makefile hook.
 */
function dbc_utils_drush_make_validate_info($info) {
  return _dbc_utils_make_validate_info($info);
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
  $valid_option_values = array('default', 'develop', 'release');

  if ($option && in_array($option, $valid_option_values)) {
    $repos = drush_get_option('dbc-github-org', 'DBCDK');
    if (drush_get_option('ding2-dev', FALSE)) {
      return drush_set_error('DBC_UTILS_ERROR', dt('--ding2-dev is not compatible with --dbc-modules.'));
    }
    drush_log(dt("Checking out $option branches of $repos Github organization repositories."), 'ok');

    foreach ($info['projects'] as $name => &$spec) {
      // Only consider git repositories.
      if (!empty($spec['download']['type']) && $spec['download']['type'] == 'git') {
        // Only mess with repos from the right organisations on
        // github. The [/:] makes this work with both http and git
        // protocol specifications.
        if (preg_match("{github\.com[/:]($repos)/}i", $spec['download']['url'])) {
          $branches = array();
          $releases = array();

          // Locate branches and release branches (git flow) on the repository.
          drush_shell_exec('git ls-remote --heads %s', $spec['download']['url']);
          foreach (drush_shell_exec_output() as $branch) {
            $branches[] = preg_replace('@.*refs/heads/(.*)@', '$1', $branch);
            if (preg_match('@.*refs/heads/release/(.*)@', $branch, $matches)) {
              $releases = $matches[0];
            }
          }

          // If we want the master branch for modules select it (actually unset
          // branches and let GitHub what the master branch is.
          if ($option == 'default') {
            if (isset($spec['download']['branch'])) {
              unset($spec['download']['branch']);
            }
          }

          // If we want the develop branch for modules select it if it exists -
          // otherwise select the default branch.
          if ($option == 'develop') {
            if (in_array('develop', $branches)) {
              $spec['download']['branch'] = 'develop';
            }
            elseif (isset($spec['download']['branch'])) {
              unset($spec['download']['branch']);
            }
          }

          // If we want a release branch for modules select the most recent
          // release branch (based on the version number) - if no release
          // branches exists select the default branch.
          if ($option == 'release') {
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

          drush_log(dt('Using @branch for @repos.',
              array(
                '@branch' => $spec['download']['branch'] ? $spec['download']['branch'] : 'the default branch',
                '@repos' => $spec['download']['url'],
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

  return $info;
}
