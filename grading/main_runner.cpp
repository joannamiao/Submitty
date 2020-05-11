#include <unistd.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <cassert>
#include <algorithm>

#include "default_config.h"
#include "execute.h"

#include <limits>
#include <tclap/CmdLine.h>

// =====================================================================
// =====================================================================

std::string addRedirectsToCommand(std::string this_command, std::string which){
  // Check to see if the instructor is already capturing STDIN
  if(this_command.find("1>") == std::string::npos){

    bool found_redirect = false;
    size_t position = this_command.find(">");

    // Check to see if they are using > rather than 1>
    // Note: escaped > characters don't count.
    while(position != std::string::npos){
      // Check to see if the character was escaped.
      // If it wasn't, break the loop because it represents a redirect.
      if(this_command[position-1] != '\\'){
        found_redirect=true;
        break;
      }
      position = this_command.find(">",position+1);
    }

    // Append a redirect if there isn't one already.
    if(found_redirect == false){
      this_command = this_command + " 1>STDOUT" + which + ".txt";
    }
  }

  if(this_command.find("2>") == std::string::npos){
    this_command += " 2>STDERR" + which + ".txt";
  }

  return this_command;
}

void executeSetOfCommands(std::vector<std::string> setOfCommands,  
                          std::vector<nlohmann::json> actions, 
                          std::vector<nlohmann::json> dispatcher_actions,
                          bool windowed,
                          std::string display_variable, 
                          TestCase testcase,
                          std::string logfile,  
                          nlohmann::json config_json,
                          int test_number){

  if ( setOfCommands.size() > 0 ) {
            
    std::cout << "========================================================" << std::endl;

    std::cout << "TEST #" << test_number << std::endl;
    std::cout << "TITLE " << testcase.getTitle() << std::endl;

    for (int command_number = 0; command_number < setOfCommands.size();  command_number++){
      std::string command = setOfCommands[command_number];

      assert (command != "MISSING COMMAND");
      assert (command != "");
        
      std::string which = "";
      if (setOfCommands.size() > 1) {
        which = "_" + std::to_string(command_number);
      }

      command = addRedirectsToCommand(command, which);

      int exit_no = execute(command,
                            actions,
                            dispatcher_actions,
                            logfile,
                            testcase.get_test_case_limits(),
                            config_json.value("resource_limits",nlohmann::json()),
                            config_json,
                            windowed,
                            display_variable,
                            testcase.has_timestamped_stdout());
    }
    std::cout << "========================================================" << std::endl;
    std::cout << "FINISHED TEST #" << test_number << std::endl;
  }
}

void runATestCase(TestCase &my_testcase, nlohmann::json &config_json, nlohmann::json::iterator &tc, int which_testcase, const std::string &docker_name, int test_case_to_run, const std::string &generation_type, bool windowed, const std::string &display_variable, int offset) {
    std::vector<std::string> commands;
    std::vector<nlohmann::json> actions;
    std::vector<nlohmann::json> dispatcher_actions;
    
    if (my_testcase.isFileCheck() || my_testcase.isCompilation()){
      return;
    }

    if(test_case_to_run != -1 &&  test_case_to_run != which_testcase){
      return;
    }

    if ( generation_type == "input" ) {
      commands = my_testcase.getInputGeneratorCommands();
    } else {
      commands = (generation_type == "output" ? my_testcase.getSolutionCommands() : my_testcase.getCommands());
      //using getID here since nothing else here uses it - should it be made a bit more clear?
      actions  = mapOrArrayOfMaps((*tc)[which_testcase - offset],"actions");
      dispatcher_actions = mapOrArrayOfMaps((*tc)[which_testcase - offset],"dispatcher_actions");

      if(generation_type != "output"){
        assert (commands.size() > 0);
      }
    }

    executeSetOfCommands(commands, actions, dispatcher_actions, windowed, display_variable, my_testcase, "execute_logfile.txt", config_json, which_testcase);
}

int main(int argc, char *argv[]) {
  std::cout << "Running User Code..." << std::endl;
  std::string hw_id = "";
  std::string rcsid = "";
  int subnum = -1;
  std::string time_of_submission = "";
  std::string docker_name = "";
  //If test_case_to_run isn't passed in as a parameter, all testcases are run.
  int test_case_to_run = -1;
  std::string display_variable = "";
  std::string generation_type = "";

  TCLAP::CmdLine cmd("Submitty's main runner program.", ' ', "0.9");
  TCLAP::UnlabeledValueArg<std::string> homework_id_argument("homework_id", "The unique id for this gradeable", true, "", "string" , cmd);
  TCLAP::UnlabeledValueArg<std::string> student_id_argument("student_id", "The unique id for this student", true, "", "string" , cmd);
  TCLAP::UnlabeledValueArg<int> submission_number_argument("submission_number", "The numeric value for this assignment attempt", true, -1, "integer" , cmd);
  TCLAP::UnlabeledValueArg<std::string> submission_time_argument("submission_time", "The time at which this submissionw as made", true, "", "string" , cmd);
  TCLAP::ValueArg<int> testcase_to_run_argument("t", "testcase", "The testcase to run. Pass -1 to run all testcases.", false, -1, "int", cmd);
  TCLAP::ValueArg<std::string> docker_name_argument("c", "container_name", "The name of the container this attempt is being run in.", false, "", "string", cmd);
  TCLAP::ValueArg<std::string> display_variable_argument("d", "display", "The display to be used for this testcase.", false, "NO_DISPLAY_SET", "string", cmd);
  TCLAP::ValueArg<std::string> generation_type_argument("g", "generation_type", "The type of generation", false, "", "string", cmd);
  
  

  //parse arguments.
  try {
    cmd.parse(argc, argv);
    hw_id = homework_id_argument.getValue();
    rcsid = student_id_argument.getValue();
    subnum = submission_number_argument.getValue();
    time_of_submission = submission_time_argument.getValue();
    docker_name = docker_name_argument.getValue();
    test_case_to_run = testcase_to_run_argument.getValue();
    display_variable = display_variable_argument.getValue();
    generation_type = generation_type_argument.getValue();
  }
  catch (TCLAP::ArgException &e)  // catch any exceptions
  { 
    std::cerr << "INCORRECT ARGUMENTS TO RUNNER" << std::endl;
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; 
    return 1;
  }

  // LOAD HW CONFIGURATION JSON
  nlohmann::json config_json = LoadAndProcessConfigJSON(rcsid);

  // nlohmann::json grading_parameters = config_json.value("grading_parameters",nlohmann::json::object());
  // int AUTO_POINTS         = grading_parameters.value("AUTO_POINTS",0);
  // int EXTRA_CREDIT_POINTS = grading_parameters.value("EXTRA_CREDIT_POINTS",0);
  // int TA_POINTS           = grading_parameters.value("TA_POINTS",0);
  // int TOTAL_POINTS        = grading_parameters.value("TOTAL_POINTS",AUTO_POINTS+TA_POINTS);

  // necessary since the untrusted user does not have a home directory
  setenv("DYNAMORIO_CONFIGDIR", ".", 1);

  system("find . -type f -exec ls -sh {} +");

  // Run each test case and create output files
  std::vector<std::string> required_capabilities = stringOrArrayOfStrings(config_json, "required_capabilities");

  bool windowed = false;
  if (std::find(required_capabilities.begin(), required_capabilities.end(), "windowed") != required_capabilities.end()){
    windowed = true;
  }

  nlohmann::json::iterator tc = config_json.find("testcases");
  nlohmann::json::iterator ic = config_json.find("item_pool");
  //TODO: Do we want to check if there are actually test cases in the item pool earlier (or maybe have a method to print the message on the first run of a test case in general)?
  //(to avoid misleading message)
  assert (tc != config_json.end() || ic != config_json.end());
  std::cout << "Test statement" << std::endl;
  if(test_case_to_run != -1){
    //testcases begin counting at 1 and end at tc->size()
    assert (test_case_to_run <= tc->size());
  }else{
    std::cout << "Running all testcases in a single run." << std::endl;
  }
  unsigned int which_testcase;
  if(tc != config_json.end()) {
    for (which_testcase = 1; which_testcase <= tc->size(); which_testcase++) {
      TestCase my_testcase(config_json, which_testcase-1, docker_name);
      runATestCase(my_testcase, config_json, 
                  tc, which_testcase, 
                  docker_name, 
                  test_case_to_run, 
                  generation_type, 
                  windowed, display_variable, 0);
    }
  }

  if(ic != config_json.end()) {
    unsigned int offset = which_testcase;
    nlohmann::json::iterator notebook = config_json.find("notebook");
    std::set<std::string> selectedItems;
    if(notebook != config_json.end()) { //should this be an assert instead since we should only have items if we have items in a notebook?
      for(unsigned int x = 0; x < notebook->size(); x++) {
        nlohmann::json notebookItem = (*notebook)[x];
        if(notebookItem["type"] == "item" && notebookItem.find("points") != notebookItem.end()) { //we never grab points from test cases, right?
          nlohmann::json pool = notebookItem["from_pool"];
          //TODO: RANDOMALLY SELECT HERE
          int result = 0;
          std::string name = pool[result];
          selectedItems.insert(name);
        }
      }
      for (unsigned int x = 0; x < ic->size(); x++) {
        nlohmann::json item = (*ic)[x];
        assert(item.find("item_name") != item.end());
        if(selectedItems.find(item["item_name"]) != selectedItems.end()) {
          tc = item.find("testcases");
          //do we want to validate the number of points in the test case here?
          for(unsigned int z = 0; z < tc->size(); z++) {
            TestCase my_testcase((*ic)[x], which_testcase - 1, docker_name, offset);
            runATestCase(my_testcase, config_json, 
                          tc, which_testcase++, 
                          docker_name, 
                          test_case_to_run, 
                          generation_type, 
                          windowed, display_variable, offset);
          }
        }
      }
    }
    
  }

  return 0;
}

// =====================================================================
// =====================================================================
