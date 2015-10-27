<?php

/**
 * @file
 *   Simpletest module drush integration.
 */

/**
 * Implementation of hook_drush_command().
 */
function jenkins_drush_command() {
  $items = array();

  $items['jenkins-run'] = array(
    'description' => "Run tests. Note that you must use the --uri option.",
    'arguments' => array(
      'targets' => 'A test class, a test group. If omitted, a list of test classes and test groups is presented. Delimit multiple targets using commas.',
    ),
    'examples' => array(
      'jenkins-run' => 'List all available classes and groups.',
      'sudo -u apache jenkins-run --all' => 'Run all available tests. Avoid permission related failures by running as web server user.',
      'jenkins-run XMLRPCBasicTestCase' => 'Run one test class.',
      'jenkins-run XML-RPC' => 'Run all classes in a XML-RPC group.',
      'jenkins-run XML-RPC,Filter' => 'Run all tests from multiple groups/classes.',
      'jenkins-run XMLRPCBasicTestCase --methods="testListMethods, testInvalidMessageParsing"' => 'Run particular methods in the specified class or group.',
    ),
    'options' => array(
      'all' => 'Run all available tests',
      'methods' => 'A comma delimited list of methods that should be run within the test class. Defaults to all methods.',
      'dirty' => 'Skip cleanup of temporary tables and files. Helpful for reading debug() messages and other post-mortem forensics.',
      'xml' => 'Output verbose test results to a specified directory using the JUnit test reporting format. Useful for integrating with Jenkins.'

    ),
    'drupal dependencies' => array('simpletest'),
    // If you DRUSH_BOOTSTRAP_DRUPAL_LOGIN, you fall victim to http://drupal.org/node/974768. We'd like
    // to not bootstrap at all but simpletest uses Drupal to discover test classes,
    // cache the lists of tests, file_prepare_directory(), variable lookup like
    // httpauth creds, copy pre-built registry table from testing side, etc.
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  $items['jenkins-clean'] = array(
    'description' => "Clean temporary tables and files.",
    'drupal dependencies' => array('simpletest'),
  );

  return $items;
}

/**
 * Command argument complete callback.
 *
 * @return
 *  Array of test classes and groups.
 */
function jenkins_jenkins_run_complete() {
  if (drush_bootstrap_max(DRUSH_BOOTSTRAP_DRUPAL_FULL) == DRUSH_BOOTSTRAP_DRUPAL_FULL && module_exists('simpletest')) {
    // Retrieve all tests and groups.
    list($groups, $all_tests) = drush_jenkins_get_all_tests();
    return array('values' => array_keys($groups) + $all_tests);
  }
}

// Command callback
function drush_jenkins_clean() {
  return simpletest_clean_environment();
}

// Validate hook
function drush_jenkins_run_validate($specs = NULL) {
  if (!drush_get_option('uri')) {
    // No longer needed?
    // return drush_set_error(dt("You must specify this site's URL using the --uri parameter."));
  }
}

/**
 * Jenkins-run command callback.
 *
 * @specs
 *   A comma delimited string of test classes or group names.
 */
function drush_jenkins_run($specs = NULL) {
  if (drush_drupal_major_version() > 7) {
    cache()->delete('simpletest');
  }
  else {
    cache_clear_all('simpletest', 'cache');
  }

  // Retrieve all tests and groups.
  list($groups, $all_tests) = drush_jenkins_get_all_tests();

  if (drush_get_option('all')) {
    // Run all tests.
    foreach (array_keys($groups) as $group) {
      foreach (array_keys($groups[$group]) as $class) {
        //drush_invoke_process('@self', 'jenkins-run', array($class));
        simpletest_drush_run_jenkins($class);
      }
    }
    if (!drush_get_option('dirty')) {
      simpletest_clean_environment();
    }
    return;
  }
  elseif (empty($specs)) {
    // No argument so list all groups/classes.
    return drush_jenkins_list($groups);
  }
  elseif (strpos($specs, ',') === FALSE && in_array($specs, $all_tests)) {
    // We got one, valid class. Good, now run it.
    simpletest_drush_run_jenkins($specs);
    if (!drush_get_option('dirty')) {
      simpletest_clean_environment();
    }
    return;
  }

  // See if we got a list of specs.
  foreach (explode(',', $specs) as $spec) {
    $spec = trim($spec);
    if (in_array($spec, $all_tests)) {
      // Specific test class specified.
      drush_jenkins_run_class($spec);
    }
    if (isset($groups[$spec])) {
      // Specific group specified.
      foreach (array_keys($groups[$spec]) as $class) {
        drush_jenkins_run_class($class);
      }
    }
  }
}

/*
 * Run a test class via drush_invoke_process()
 * @param string $class
 *   The test class to run.
 *
 * @return
 *   If the command could not be completed successfully, FALSE.
 *   If the command was completed, this will return an associative
 *   array containing the results of the API call.
 *   @see drush_backend_get_result()
 */
function drush_jenkins_run_class($class) {
  $backend_options = array('output-label' => "$class ", 'integrate' => TRUE, 'output' => TRUE);
  $return = drush_invoke_process('@self', 'jenkins-run', array($class), drush_redispatch_get_options(), $backend_options);
  return $return;
}

/**
 * Run a single test and display any failure messages.
 */
function simpletest_drush_run_jenkins($class) {
  if (drush_drupal_major_version() >= 7) {
    $test_id = db_insert('simpletest_test_id')
    ->useDefaults(array('test_id'))
    ->execute();
  }
  else {
    db_query('INSERT INTO {simpletest_test_id} (test_id) VALUES (default)');
    $test_id = db_last_insert_id('simpletest_test_id', 'test_id');
  }
  $test = new $class($test_id);
  
  $start_time = microtime(TRUE);
  
  if ($methods_string = drush_get_option('methods')) {
    foreach (explode(',', $methods_string) as $method) {
      $methods[] = trim($method);
    }
    $test->run($methods);
  }
  else {
    $test->run();
  }
  
  $test_time = microtime(TRUE) - $start_time;

  $info = $test->getInfo();
  $status = ((isset($test->results['#fail']) && $test->results['#fail'] > 0)
          || (isset($test->results['#exception']) && $test->results['#exception'] > 0) ? 'error' : 'ok');
  drush_log($info['name'] . ' ' . _simpletest_format_summary_line($test->results), $status);

  if ($dir = drush_get_option('xml')) {
    drush_log('Creating XML file at directory ' . $dir, 'ok');
    drush_jenkins_xml_results($test_id, $dir, $info['name'], $info['group'], $test_time);
  }

  // If there were some failed tests show them.
  if ($status === 'error') {
    if (drush_drupal_major_version() >= 7) {
      $args = array(':test_id' => $test_id);
      $result = db_query("SELECT * FROM {simpletest} WHERE test_id = :test_id AND status IN ('exception', 'fail') ORDER BY test_class, message_id", $args);
      foreach($result as $record) {
        drush_set_error('DRUSH_TEST_FAIL', dt("Test !function failed: !message", array('!function' => $record->function, '!message' => $record->message)));
      }
    }
    else {
      $result = db_query("SELECT * FROM {simpletest} WHERE test_id = %d AND status IN ('exception', 'fail') ORDER BY test_class, message_id", $test_id);
      while ($row = db_fetch_object($result)) {
        drush_set_error('DRUSH_TEST_FAIL', dt("Test !function failed: !message", array('!function' => $row->function, '!message' => $row->message)));
      }
    }
  }
}

/**
 * Retrieve all test groups and sanitize their names to make them command-line
 * friendly.
 */
function simpletest_drush_jenkins_groups($tests) {
  $groups = array();
  foreach (simpletest_categorize_tests($tests) as $name => $group) {
    $sanitized = strtr($name, array(' ' => ''));
    $groups[$sanitized] = $group;
  }
  return $groups;
}

// Print a listing of all available tests
function drush_jenkins_list($groups) {
  $rows[] = array(dt('Command'), dt('Description'));
  $rows[] = array('-------', '-----------');
  foreach ($groups as $group_name => $group_tests) {
    foreach ($group_tests as $test_class => $test_info) {
      if (!isset($rows[$test_info['group']])) {
        $rows[$test_info['group']] = array($group_name, $test_info['group']);
      }
      $rows[] = array("  {$test_class}", "  {$test_info['name']}");
    }
  }
  return drush_print_table($rows, TRUE);
}

function drush_jenkins_get_all_tests() {
  if (function_exists('simpletest_get_all_tests')) {
    $all_tests = simpletest_get_all_tests();
    $groups = simpletest_drush_test_groups($all_tests);
  }
  else {
    $groups = simpletest_test_get_all();
    $all_tests = array();
    foreach ($groups as $group) {
      $all_tests = array_merge($all_tests, array_keys($group));
    }
  }
  return array($groups, $all_tests);
}

/*
 * Display test results.
 */
function drush_jenkins_xml_results($test_id, $dir, $name, $group, $time) {
  $dir = is_string($dir) ? $dir : '.';

  $results_map = array(
    'pass' => 'Pass',
    'fail' => 'Fail',
    'exception' => 'Exception',
    'debug' => 'Debug',
  );

  if (drush_drupal_major_version() >= 7) {
    $results = db_query("SELECT * FROM {simpletest} WHERE test_id = :test_id ORDER BY test_class, message_id", array(':test_id' => $test_id));
  }
  else {
    $result = db_query("SELECT * FROM {simpletest} WHERE test_id = %d ORDER BY test_class, message_id", $test_id);
    $results = array();
    while ($row = db_fetch_object($result)) {
      $results[] = $row;
    }
  }

  $cases = 0;
  $errors = 0;
  $failures = 0;

  foreach ($results as $result) {
    if (isset($results_map[$result->status])) {
      $func = substr($result->function, strpos($result->function, '>') + 1);
      $func = substr($func, 0, strlen($func) - 2);
      
      if (isset($result->test_class)) $case = $result->test_class;
      $classname = $group . '.' . $case;
        
      if ($result->status == 'pass' || $result->status == 'fail') {
        $classes[$case][$func]['classname'] = $classname;
        $classes[$case][$func]['status'] = $result->status;
        $classes[$case][$func]['text'] .= $result->status . ' [' . $result->message_group . '] ' . strip_tags($result->message) . "\n";
      }

      if ($result->status == 'fail') {
        $classes[$case][$func]['failures'][] = array('type' => $result->message_group, 'message' => strip_tags($result->message));
        $failures++;
      }

      if ($result->status == 'exception') {
        $classes[$case]['Exceptions']['classname'] = $classname;
        $classes[$case]['Exceptions']['errors'][] = array('type' => $result->function, 'message' => strip_tags($result->message));
        $errors++;
      }
    }
  }

  foreach ($classes as $key => $class) {
    $doc = new DomDocument('1.0');
    $root = $doc->createElement('testsuite');
    $root = $doc->appendChild($root);
    $root->setAttribute('id', $test_id);
    $root->setAttribute('name', $name);
  
    foreach ($class as $function => $testcase) {
      $case = $doc->createElement('testcase');
      $case->setAttribute('classname', $testcase['classname']);
      $case->setAttribute('name', $function);
      
      if (isset($testcase['failures'])) {
        $fail = $doc->createElement('failure');
        $fail->setAttribute('message', 'One or more steps failed');
        $case->appendChild($fail);
      }
  
      if (isset($testcase['errors'])) {
        $error = $doc->createElement('error', $testcase['errors'][0]['message']);
        $error->setAttribute('message', $testcase['errors'][0]['type']);
        $case->appendChild($error);
      }
  
      if (isset($testcase['text'])) {
        $out = $doc->createElement('system-out');
        $text = $doc->createCDATASection($testcase['text']);
        $out->appendChild($text);
        $case->appendChild($out);
      }
      
      $root->appendChild($case);
      
      $cases++;
    }
  
    $root->setAttribute('tests', $cases);
    $root->setAttribute('errors', $errors);
    $root->setAttribute('failures', $failures);
    $root->setAttribute('time', $time);
    $root->setAttribute('timestamp', date('c'));
    file_put_contents($dir . '/' . $key . '.xml', $doc->saveXML());
  }
}


function jenkins_test_finished($results) {
  drush_print('hook_test_finished called', 2);
  drush_log('hook_test_finished called', 'ok');
}
