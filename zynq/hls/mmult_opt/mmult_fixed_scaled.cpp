#include <stdio.h>
#include <stdlib.h>

#include "mmult.h"

// --------------------------------------------------------------------
/* The images are scaled from 16x16 pixels to 12x12 pixels */
// function to be accelerated in HW wrapped with AXI4-Stream interface
void mmult_hw (AXI_VAL in_stream[IS_SIZE], AXI_VAL out_stream[OS_SIZE]) {
#pragma HLS INTERFACE s_axilite port=return     bundle=CONTROL_BUS
#pragma HLS INTERFACE axis      port=in_stream
#pragma HLS INTERFACE axis      port=out_stream

	// Assertions (to avoid out of array bound writes)
	assert(BATCH%TILING==0);
	assert(FEAT%W_WIDTH_RATIO==0);
	assert(FEAT%IN_WIDTH_RATIO==0);
	assert((BATCH*CLASSES)%OUT_WIDTH_RATIO==0);

	// Hardware memory buffers
	out_T offset_buf[CLASSES];
	w_T weight_buf[CLASSES][FEAT];
	in_T in_buf[TILING][FEAT];
	out_T out_buf[TILING][CLASSES];
	axi_T input_packet_array[TILING][FEAT/IN_WIDTH_RATIO];
	axi_T weight_packet_array[FEAT/W_WIDTH_RATIO];
	axi_T output_packet_array[TILING][5]; /* I am desperate here, but hard coding needs to be avoided */

#pragma HLS ARRAY_PARTITION variable=weight_buf block factor=72 dim=2
#pragma HLS ARRAY_PARTITION variable=in_buf block factor=72 dim=2

	// Input and output AXI stream indices
	int is_idx = 0;
	int os_idx = 0;
	int idx    = 0;

	int i, j, k, w;
	// Stream in offset vector
	// CSE548 DONE
	UNPACK_OFFSET_LOOP : for(int i = 0; i < CLASSES - OUT_WIDTH_RATIO; i+=OUT_WIDTH_RATIO) {
		axi_T packet = pop_stream(in_stream[is_idx++]);
		UNPACK_OFFSET_ITER : for(int w = 0; w < OUT_WIDTH_RATIO; w++) {
			out_bit_T bits  = (packet>>(w*OUT_WIDTH));
			offset_buf[i+w] = *((out_T*) &bits) & ((1ULL<<OUT_WIDTH)-1);
		}
	}
	axi_T packet = pop_stream(in_stream[is_idx++]);
	UNPACK_FINISH_OFFSET: for (int i = CLASSES-OUT_WIDTH_RATIO; i < CLASSES; i++) {
		out_bit_T bits = (packet>>((i%OUT_WIDTH_RATIO)*OUT_WIDTH));
		offset_buf[i]  = *((out_T*) &bits) & ((1ULL<<OUT_WIDTH)-1);
	}


	/* START FROM HERE : AND VERIFY THE ABOVE */
	// Stream in weight matrix
	// CSE548 DONE
	UNPACK_WEIGHT_LOOP_CLASS : for(int i=0; i<CLASSES; i++) {
		UNPACK_WEIGHT_LOOP_FEAT : for(int j=0, idx=0; j<FEAT; j+=W_WIDTH_RATIO, idx++) {
#pragma HLS PIPELINE II=1
			weight_packet_array[idx] = pop_stream(in_stream[is_idx++]);
			UNPACK_WEIGHT: for (int w = 0; w < W_WIDTH_RATIO; w++) {
				w_bit_T bits = (weight_packet_array[idx]>>(w*W_WIDTH));
				weight_buf[i][j+w] = *((w_T*) &bits) & ((1ULL<<W_WIDTH)-1);
			}
		}
	}

	// Iterate over tiles
	LT: for (int t = 0; t < BATCH; t+=TILING) {

		// Stream in input tile
		// CSE548 DONE
		UNPACK_INPUT_LOOP_TILE : for(int i=0; i<TILING; i++) {
#pragma HLS PIPELINE II=1
			UNPACK_INPUT_LOOP_FEAT : for(int j=0, idx = 0; j<FEAT; j+=IN_WIDTH_RATIO, idx++) {
				input_packet_array[i][idx] = \
				pop_stream(in_stream[is_idx++]);

				UNPACK_INPUT: for (int w = 0; w < IN_WIDTH_RATIO; w++) {
					in_bit_T bits = (input_packet_array[i][idx]>>(w*IN_WIDTH));
					in_buf[i][j+w] = *((in_T*) &bits) & ((1ULL<<IN_WIDTH)-1);
				}
			}
		}

		// Perform matrix multiplication
		L1: for (int i = 0; i < TILING; i++) {
			// Iterate over output classes
			L2: for (int j = 0; j < CLASSES; j++) {
#pragma HLS PIPELINE II=1
				// Perform the dot product
				out_T tmp = offset_buf[j];
				L3: for(int k = 0; k < FEAT; k++) {
					out_T mult = in_buf[i][k] * weight_buf[j][k];
					tmp += mult;
#pragma HLS RESOURCE variable=mult core=Mul_LUT
				}
				out_buf[i][j] = tmp;
			}
		}

		// Stream out output matrix
		// CSE548 DONE
		PACK_OUTPUT_LOOP_TILE : for(int i=0; i<TILING; i++) {
			#pragma HLS PIPELINE II=1
			PACK_OUTPUT_LOOP_CLASS : for(int j=0, idx = 0; j<CLASSES-OUT_WIDTH_RATIO; j+=OUT_WIDTH_RATIO, idx++) {
				output_packet_array[i][idx] = 0;
				PACK_OUTPUT_ITER : for (int w = 0; w < OUT_WIDTH_RATIO; w++) {
					out_bit_T bits = *((out_bit_T*) &out_buf[i][j+w]);
					output_packet_array[i][idx] |= (bits & ((1ULL<<OUT_WIDTH)-1)) << (w*OUT_WIDTH);
				}
				out_stream[os_idx++] = push_stream(output_packet_array[i][idx],0);
			}
			axi_T packet = 0;
			PACK_FINISH_OUTPUT : for(int j = CLASSES-OUT_WIDTH_RATIO; j < CLASSES; j++) {
				out_bit_T bits = *((out_bit_T*) &out_buf[i][j]);
				packet |= (bits & ((1ULL<<OUT_WIDTH)-1)) << ((j%OUT_WIDTH_RATIO)*OUT_WIDTH);
			}
			out_stream[os_idx++] = push_stream(packet,os_idx==(OS_SIZE));
		}
	}
}


// --------------------------------------------------------
// functions to insert and extract elements from an axi stream
// includes conversion to correct data type
axi_T pop_stream(AXI_VAL const &e)
{
#pragma HLS INLINE

	axi_T ret = e.data;

	volatile ap_uint<sizeof(axi_T)> strb = e.strb;
	volatile ap_uint<sizeof(axi_T)> keep = e.keep;
	volatile ap_uint<AXI_U> user = e.user;
	volatile ap_uint<1> last = e.last;
	volatile ap_uint<AXI_TI> id = e.id;
	volatile ap_uint<AXI_TD> dest = e.dest;

	return ret;
}

AXI_VAL push_stream(axi_T const &v, bool last = false)
{
#pragma HLS INLINE

	AXI_VAL e;

	e.data = v;
	e.strb = (1<<sizeof(axi_T))-1;
	e.keep = (1<<sizeof(axi_T))-1;
	e.user = 0;
	e.last = last ? 1 : 0;
	e.id = 0;
	e.dest = 0;
	return e;
}

