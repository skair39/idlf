/*
Copyright (c) 2015, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "tester/common/test_aggregator.h"
#include "tester/common/workflows_for_tests.h"  // If test needs a workflow definition
#include "tester/common/test_common_tools.h"
#include "common/time_control.h"

class test_merge_float_cpu_random : public test_base {
private:
    tested_device*            current_tested_device;
    nn_device_interface_0_t*  di;

    bool init();
    bool done();
    void cleanup();

    nn::data<float>* cpu_layer_merge( nn::data<float>& work_item1, nn::data<float>& work_item2 );

    // If test needs a workflow definition
    workflows_for_tests_base    *workflow_wrapper;
    nn_workflow_t               *workflow;

    // Add current test specific variables
    uint32_t input_size_x,
             input_size_y,
             input_size_z,
             output_size_x,
             output_size_y,
             output_size_z;

public:
    test_merge_float_cpu_random() { test_description = "merge float cpu random"; };
    ~test_merge_float_cpu_random() {};
    bool run();
};

nn::data<float>* test_merge_float_cpu_random::cpu_layer_merge( nn::data<float>& work_item1, nn::data<float>& work_item2) {
    auto batch  = work_item1.size[3];
    auto output = new nn::data<float>( output_size_z, output_size_x, output_size_y, batch );

    size_t tabXYZN[4] = { (size_t) (output_size_x), (size_t) (output_size_y), (size_t) (output_size_z), (size_t) (batch) };

    for(uint32_t n = 0; n < batch; ++n)
        for(uint32_t z = 0; z < output_size_z; ++z)
            for(uint32_t x = 0; x < output_size_x; ++x)
                for(uint32_t y = 0; y < output_size_y; ++y)
                    output->at(z, x, y, n) = ( z < input_size_z )? work_item1.at(z, x, y, n) : work_item2.at(z - input_size_z, x, y, n);

    return output;
}

bool test_merge_float_cpu_random::init() {
    bool  init_ok = true;
    test_measurement_result   init_result;
    init_result.description = "INIT: " + test_description;

    C_time_control            init_timer;

    try {
        if( devices != nullptr ) {
            current_tested_device = devices->get( "device_cpu" + dynamic_library_extension );
            di = current_tested_device->get_device_interface();
        } else  throw std::runtime_error( std::string( "Can't find aggregator of devices" ) );

        // TODO: here code of test initiation:
        // If test needs a workflow definition

        workflow_wrapper = workflows_for_tests::instance().get( "workflow_merge" );
        workflow = workflow_wrapper->init_test_workflow( di );
        if( workflow == nullptr )  throw std::runtime_error( "Workflow has not been initialized" );

        input_size_x  = workflow->input[0]->output_format->format_3d.size[0];
        input_size_y  = workflow->input[0]->output_format->format_3d.size[1];
        input_size_z  = workflow->input[0]->output_format->format_3d.size[2];
        output_size_x = workflow->input[0]->use[0].item->output_format->format_3d.size[0];
        output_size_y = workflow->input[0]->use[0].item->output_format->format_3d.size[1];
        output_size_z = workflow->input[0]->use[0].item->output_format->format_3d.size[2];

        // END test initiation
        init_ok = true;
    } catch( std::runtime_error &error ) {
        init_result << "error: " + std::string( error.what() );
        init_ok = false;
    } catch( std::exception &error ) {
        init_result << "error: " + std::string( error.what() );
        init_ok = false;
    } catch( ... ) {
        init_result << "unknown error";
        init_ok = false;
    }

    init_timer.tock();
    init_result.time_consumed = init_timer.get_time_diff();
    init_result.clocks_consumed = init_timer.get_clocks_diff();
    init_result.passed = init_ok;

    tests_results << init_result;

    return init_ok;
}

bool test_merge_float_cpu_random::run() {
    bool  run_ok = true;
    test_measurement_result   run_result;
    run_result.description = "RUN SUMMARY: " + test_description;

    C_time_control  run_timer;

    std::cout << "-> Testing: " << test_description << std::endl;

    try {
        if( !init() ) throw std::runtime_error( "init() returns false so can't run test" );
        run_timer.tick();   //start time measurement
        run_result << std::string( "run test with " + current_tested_device->get_device_description() );

        NN_WORKLOAD_DATA_TYPE input_formats[] = { NN_WORKLOAD_DATA_TYPE_F32_ZXY_BATCH, NN_WORKLOAD_DATA_TYPE_F32_ZXY_BATCH };
        NN_WORKLOAD_DATA_TYPE output_format   = NN_WORKLOAD_DATA_TYPE_F32_ZXY_BATCH;

        for( auto batch : { 1, 8, 48 } ) {
            // ---------------------------------------------------------------------------------------------------------
            {   // simple sample pattern of test with time measuring:
                bool local_ok = true;
                test_measurement_result local_result;
                local_result.description = "RUN PART: (batch " + std::to_string( batch ) + ") execution of " + test_description;
                C_time_control  local_timer;
                // begin local test

                auto input1 = new nn::data<float>( input_size_z, input_size_x, input_size_y, batch );
                if(input1 == nullptr)   throw std::runtime_error("unable to create input1 for batch = " +std::to_string(batch));

                auto input2 = new nn::data<float>( input_size_z, input_size_x, input_size_y, batch );
                if(input2 == nullptr) {
                    delete input1;
                    throw std::runtime_error("unable to create input2 for batch = " +std::to_string(batch));
                }

                auto workload_output = new nn::data<float>( output_size_z, output_size_x, output_size_y, batch );
                if(workload_output == nullptr) {
                    delete input1;
                    delete input2;
                    throw std::runtime_error("unable to create workload_output for batch = " +std::to_string(batch));
                }

                nn_data_populate( workload_output, 0.0f );
                nn_data_populate( input1, -255.0f, 255.0f );
                nn_data_populate( input2, -255.0f, 255.0f );

                nn_workload_t *workload = nullptr;
                nn_data_t *input_array[2] = { input1, input2 };
                nn::data<float> *output_array_cmpl[1] = { nn::data_cast<float, 0>(workload_output) };

                auto status = di->workflow_compile_function( &workload, di->device, workflow, input_formats, &output_format, batch );
                if( !workload ) throw std::runtime_error( "workload compilation failed for batch = " + std::to_string( batch )
                                                            + " status: " + std::to_string( status ) );

                di->workload_execute_function( workload, reinterpret_cast<void**>(input_array), reinterpret_cast<void**>(output_array_cmpl), &status );

                auto naive_output = cpu_layer_merge( *input1, *input2 );
                if(naive_output == nullptr)   {
                    delete input1;
                    delete input2;
                    delete workload_output;
                    throw std::runtime_error("unable to create naive_output for batch = " +std::to_string(batch));
                }

                local_ok = compare_data(workload_output, naive_output);

                // end of local test
                // summary:
                local_timer.tock();
                local_result.time_consumed = local_timer.get_time_diff();
                local_result.clocks_consumed = local_timer.get_clocks_diff();
                local_result.passed = local_ok;
                tests_results << local_result;

                run_ok = run_ok && local_ok;

                if( input1 )          delete input1;
                if( input2 )          delete input2;
                if( workload_output ) delete workload_output;
                if( naive_output )    delete naive_output;
                if( workload )        delete workload;

            } // The pattern, of complex instruction above, can be multiplied
            // END of run tests
            // ---------------------------------------------------------------------------------------------------------
        }
    } catch( std::runtime_error &error ) {
        run_result << "error: " + std::string( error.what() );
        run_ok = false;
    } catch( std::exception &error ) {
        run_result << "error: " + std::string( error.what() );
        run_ok = false;
    } catch( ... ) {
        run_result << "unknown error";
        run_ok = false;
    }

    run_timer.tock();
    run_result.time_consumed = run_timer.get_time_diff();
    run_result.clocks_consumed = run_timer.get_clocks_diff();

    run_result.passed = run_ok;
    tests_results << run_result;
    if( !done() ) run_ok = false;
    std::cout << "<- Test " << (run_ok ? "passed" : "failed") << std::endl;
    return run_ok;
}

bool test_merge_float_cpu_random::done() {
    bool  done_ok = true;
    test_measurement_result   done_result;
    done_result.description = "DONE: " + test_description;

    C_time_control            done_timer;

    try {
        // TODO: here clean up after the test
        // If the test used definition of workflow:
        if( workflow_wrapper != nullptr ) workflow_wrapper->cleanup();

        // END of cleaning
        done_ok = true;
    } catch( std::runtime_error &error ) {
        done_result << "error: " + std::string( error.what() );
        done_ok = false;
    } catch( std::exception &error ) {
        done_result << "error: " + std::string( error.what() );
        done_ok = false;
    } catch( ... ) {
        done_result << "unknown error";
        done_ok = false;
    }

    done_timer.tock();
    done_result.time_consumed = done_timer.get_time_diff();
    done_result.clocks_consumed = done_timer.get_clocks_diff();

    done_result.passed = done_ok;
    tests_results << done_result;

    return done_ok;
}

// Code below creates 'attach_' object in anonymous namespace at global scope.
// This ensures, that object itself is not visible to other compilation units
// and it's constructor is ran befor main execution starts.
// The sole function of this construction is attaching this test to
// library of tests (singleton command pattern).

namespace {
    struct attach {
        test_merge_float_cpu_random test;
        attach() {
            test_aggregator::instance().add( &test );
        }
    };
    attach attach_;
}