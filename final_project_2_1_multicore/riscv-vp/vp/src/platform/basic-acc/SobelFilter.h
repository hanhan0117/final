#ifndef SOBEL_FILTER_H_
#define SOBEL_FILTER_H_
#include <systemc>
#include <cmath>
#include <iomanip>
using namespace sc_core;

#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "filter_def.h"

struct SobelFilter : public sc_module
{
  tlm_utils::simple_target_socket<SobelFilter> tsock;

  sc_fifo<unsigned char> i_r;
  sc_fifo<unsigned char> i_g;
  sc_fifo<unsigned char> i_b;
  sc_fifo<unsigned char> i_d;
  sc_fifo<int> o_result;

  SC_HAS_PROCESS(SobelFilter);

  SobelFilter(sc_module_name n) : sc_module(n),
                                  tsock("t_skt"),
                                  base_offset(0)
  {
    tsock.register_b_transport(this, &SobelFilter::blocking_transport);
    SC_THREAD(do_filter);
  }

  ~SobelFilter()
  {
  }

  // int val[MASK_N];
  unsigned int base_offset;
  int arr1[64];
  int arr2[64];
  int output1[128];

  void do_filter()
  {
    {
      wait(10, SC_NS);
    }
    int done = 0;
    sc_time initial_time = sc_time_stamp();
    for (int i = 0; i < 64; i = i + 4)
    {
      arr1[i] = i_r.read();
      arr1[i + 1] = i_g.read();
      arr1[i + 2] = i_b.read();
      arr1[i + 3] = i_d.read();
    }
    for (int i = 0; i < 64; i = i + 4)
    {
      arr2[i] = i_r.read();
      arr2[i + 1] = i_g.read();
      arr2[i + 2] = i_b.read();
      arr2[i + 3] = i_d.read();
    }
    sc_time start_time = sc_time_stamp();
    sc_time latency = start_time - initial_time;

    std::cout << "Total latency for loading input data into array: " << latency << std::endl;
    // bubblesort arr1
    for (int j = 0; j < 63; j++)
    {
      for (int i = 0; i < 63; i++)
      {
        if (arr1[i] > arr1[i + 1])
        {
          int t = arr1[i];
          arr1[i] = arr1[i + 1];
          arr1[i + 1] = t;
          wait(20, SC_NS);
        }
      }
    }
    // bubblesort arr2
    for (int j = 0; j < 63; j++)
    {
      for (int i = 0; i < 63; i++)
      {
        if (arr2[i] > arr2[i + 1])
        {
          int t = arr2[i];
          arr2[i] = arr2[i + 1];
          arr2[i + 1] = t;
          wait(20, SC_NS);
        }
      }
    }
    // merge arr1 and arr2 into ouput1 array
    int i = 0;
    int j = 0;
    int k = 0;
    while (k < 128)
    {
      if (i == 64)
      {
        for (int n = j; n < 64; n++)
        {
          output1[n + 64] = arr2[n];
          k++;
          wait(20, SC_NS);
        }
      }
      else if (j == 64)
      {
        for (int n = i; n < 64; n++)
        {
          output1[n + 64] = arr1[n];
          k++;
          wait(20, SC_NS);
        }
      }
      else
      {
        if (arr1[i] > arr2[j])
        {
          output1[k] = arr2[j];
          j++;
          k++;
          wait(10, SC_NS);
        }
        else
        {
          output1[k] = arr1[i];
          i++;
          k++;
          wait(10, SC_NS);
        }
      }
    }
    sc_time end_time = sc_time_stamp();
    latency = end_time - start_time;

    std::cout << "Total latency before sending output: " << latency << std::endl;

    // output
    for (int i = 0; i < 128; i = i + 3)
    {
      sc_dt::sc_uint<32> out;
      if (i < 126)
      {
        out.range(7, 0) = output1[i];
        out.range(15, 8) = output1[i + 1];
        out.range(23, 16) = output1[i + 2];
        out.range(31, 24) = 1;
        o_result.write(out);
      }
      else
      {
        out.range(7, 0) = output1[i];
        out.range(15, 8) = output1[i + 1];
        out.range(23, 16) = 0;
        out.range(31, 24) = 1;
        o_result.write(out);
        done = 1;
      }
    }
    if (done)
    {
      end_time = sc_time_stamp();
      latency = end_time - initial_time;

      std::cout << "Total latency: " << latency << std::endl;
    }
  }

  void blocking_transport(tlm::tlm_generic_payload &payload, sc_core::sc_time &delay)
  {
    wait(delay);
    // unsigned char *mask_ptr = payload.get_byte_enable_ptr();
    // auto len = payload.get_data_length();
    tlm::tlm_command cmd = payload.get_command();
    sc_dt::uint64 addr = payload.get_address();
    unsigned char *data_ptr = payload.get_data_ptr();

    addr -= base_offset;

    // cout << (int)data_ptr[0] << endl;
    // cout << (int)data_ptr[1] << endl;
    // cout << (int)data_ptr[2] << endl;
    word buffer;

    switch (cmd)
    {
    case tlm::TLM_READ_COMMAND:
      // cout << "READ" << endl;
      switch (addr)
      {
      case SOBEL_FILTER_RESULT_ADDR:
        buffer.uint = o_result.read();
        break;
      default:
        std::cerr << "READ Error! SobelFilter::blocking_transport: address 0x"
                  << std::setfill('0') << std::setw(8) << std::hex << addr
                  << std::dec << " is not valid" << std::endl;
      }
      data_ptr[0] = buffer.uc[0];
      data_ptr[1] = buffer.uc[1];
      data_ptr[2] = buffer.uc[2];
      data_ptr[3] = buffer.uc[3];
      break;
    case tlm::TLM_WRITE_COMMAND:
      // cout << "WRITE" << endl;
      switch (addr)
      {
      case SOBEL_FILTER_R_ADDR:
        i_r.write(data_ptr[0]);
        i_g.write(data_ptr[1]);
        i_b.write(data_ptr[2]);
        i_d.write(data_ptr[3]);
        break;
      default:
        std::cerr << "WRITE Error! SobelFilter::blocking_transport: address 0x"
                  << std::setfill('0') << std::setw(8) << std::hex << addr
                  << std::dec << " is not valid" << std::endl;
      }
      break;
    case tlm::TLM_IGNORE_COMMAND:
      payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
      return;
    default:
      payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
      return;
    }
    payload.set_response_status(tlm::TLM_OK_RESPONSE); // Always OK
  }
};
#endif
