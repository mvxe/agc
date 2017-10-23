/*
    Alpha Gamma Counter
    Copyright (C) 2017  Mario Vretenar

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

module lfs
(
	// signals
	input                 clk_i           ,  //!< clock
	input                 rstn_i          ,  //!< reset - active low
	input      [ 14-1: 0] dat_a_i         ,  //!< input data CHA
	input      [ 14-1: 0] dat_b_i         ,  //!< input data CHB
	output     [ 14-1: 0] dat_a_o         ,  //!< output data CHA
	output     [ 14-1: 0] dat_b_o         ,  //!< output data CHB
	
	// Expansion connector
	input      [  8-1: 0] exp_p_dat_i     ,  //!< exp. con. input data
	output reg [  8-1: 0] exp_p_dat_o     ,  //!< exp. con. output data
	output reg [  8-1: 0] exp_p_dir_o     ,  //!< exp. con. 1-output enable
	input      [  8-1: 0] exp_n_dat_i     ,  //!<
	output reg [  8-1: 0] exp_n_dat_o     ,  //!<
	output reg [  8-1: 0] exp_n_dir_o     ,  //!<
   
	//System bus	
	input      [ 32-1: 0] sys_addr      ,  //!< bus address
	input      [ 32-1: 0] sys_wdata     ,  //!< bus write data
	input      [  4-1: 0] sys_sel       ,  //!< bus write byte select
	input                 sys_wen       ,  //!< bus write enable
	input                 sys_ren       ,  //!< bus read enable
	output reg [ 32-1: 0] sys_rdata     ,  //!< bus read data
	output reg	      sys_err       ,  //!< bus error indicator
	output reg	      sys_ack          //!< bus acknowledge signal
	
);


reg signed [ 13: 0] out_a;		//output data ch A
reg signed [ 13: 0] out_b;		//output data ch B
wire signed  [ 13: 0] in_a;		//input data ch A
wire signed  [ 13: 0] in_b;		//input data ch B
assign dat_a_o = out_a;
assign dat_b_o = out_b;
assign in_a = dat_a_i;
assign in_b = dat_b_i;



////-------------------------------------- Main ---------------------------------------////

`define FIFO_size 200

reg signed [ 13: 0] cntr_thresh_alpha;
reg                 cntr_sign_alpha;
reg signed [ 13: 0] cntr_thresh_gamma;
reg                 cntr_sign_gamma;
reg  [ 31: 0] cntr_mintime_alpha;
reg  [ 31: 0] cntr_mintime_gamma;

reg [ 31: 0] cntr_t0_buf [0:`FIFO_size-1];
reg [ 31: 0] cntr_t1_buf [0:`FIFO_size-1];
//reg [ 31: 0] cntr_t2_buf [0:`FIFO_size-1];		//counter t2 - time between trigger and peak is not neaded anymore and has been disabled to provide more space
reg signed [ 13: 0] cntr_amp_buf [0:`FIFO_size-1];
reg          cntr_type_buf [0:`FIFO_size-1];		//0 means alpha, 1 means gamma
reg 	     cntr_isd_buf [0:`FIFO_size-1];		//indicates if this element (of all four arrays) contains any data
integer i;

reg  [ 7: 0] out_a_time_shift;				//this is used to add a time delay on channel A
reg signed [ 13: 0] out_a_fifo[0:255];			//max shift of 2040 ns
integer j;
reg signed [ 13: 0] out_b_dela;				//we delay ch B by one cycle so that for out_a_time_shift==0 they are in phase

reg          reset_fifo;
reg          reset_fifo_loc;
reg [ 31: 0] mes_lost;

reg [ 15: 0] mes_in_FIFO;
reg [ 15: 0] max_mes_in_FIFO;


reg [ 31: 0] cntr_t0;
reg signed [ 13: 0] cntr_max_alpha;
reg [ 31: 0] cntr_t1_alpha;
//reg [ 31: 0] cntr_t2_alpha;
reg cntr_alpha_ongoing;
reg cntr_alpha_saveflag;
reg signed [ 13: 0] cntr_max_gamma;
reg [ 31: 0] cntr_t1_gamma;
//reg [ 31: 0] cntr_t2_gamma;
reg cntr_gamma_ongoing;
reg cntr_gamma_saveflag;

reg          mes_received;				//this gets flipped if the last element in the FIFO has been read
reg          mes_received_loc;				//a local copy of mes_received which we check to see if flipped

always @(posedge clk_i) begin	
	if ((rstn_i == 1'b0) || (reset_fifo != reset_fifo_loc)) begin
		if(reset_fifo != reset_fifo_loc) begin
			reset_fifo_loc <= reset_fifo;
		end
		else begin
			reset_fifo_loc <= 1'd0;
			mes_received_loc <= 1'd0;
		end
	
		for (i=0; i<=`FIFO_size-1; i=i+1)	//FIFO
			cntr_isd_buf[i] <= 1'd0;	//none of the elements contain any data	
			
		for (j=0; j<=255; j=j+1)		//chB delay FIFO
			out_a_fifo[j] <= 1'd0;	
		out_b_dela <= 1'd0;
		
		mes_in_FIFO <= 16'd0;
		max_mes_in_FIFO <= 16'd0;
		mes_lost <= 32'd0;
		
		
		out_a <= 14'sd0;
		out_b <= 14'sd0;
		
		cntr_t0 <= 32'd0;
		cntr_max_alpha  <= 14'sd0;
		cntr_t1_alpha <= 32'd0;
//		cntr_t2_alpha <= 32'd0;
		cntr_alpha_ongoing <= 1'd0;
		cntr_alpha_saveflag <= 1'd0;
		cntr_max_gamma  <= 14'sd0;
		cntr_t1_gamma <= 32'd0;
//		cntr_t2_gamma <= 32'd0;
		cntr_gamma_ongoing <= 1'd0;
		cntr_gamma_saveflag <= 1'd0;
	end
	else begin
		for (j=0; j<255; j=j+1)			//chB delay FIFO
			out_a_fifo[j+1] <= out_a_fifo[j] ;
		out_a_fifo[0] <= in_a;
		out_b_dela <= in_b;
				
		if ((cntr_alpha_saveflag == 1'b0) && (cntr_gamma_saveflag == 1'b0)) begin
			cntr_t0 <= cntr_t0 + 32'd1;	//increment timestamp
			if (mes_received_loc != mes_received) begin			//last element has been read
				mes_received_loc <= mes_received;
				cntr_isd_buf[`FIFO_size-1] <= 1'd0;
				mes_in_FIFO <= mes_in_FIFO-16'd1;
			end
		end
		else if ((cntr_isd_buf[0]==1'd0) && (cntr_alpha_saveflag == 1'b1)) begin
			cntr_t0_buf[0] <= cntr_t0 - 32'd1;
			cntr_t1_buf[0] <= cntr_t1_alpha;		//FIFO first element
//			cntr_t2_buf[0] <= cntr_t2_alpha;
			cntr_amp_buf[0] <= cntr_max_alpha;
			cntr_isd_buf[0] <= 1'd1;
			cntr_type_buf[0] <= 1'd0;			//alpha
			cntr_t0 <= 32'd1;
			cntr_alpha_saveflag <= 1'b0;
			mes_in_FIFO <= mes_in_FIFO+16'd1;
		end
		else if (cntr_isd_buf[0]==1'd0) begin
			cntr_t0_buf[0] <= cntr_t0 - 32'd1;
			cntr_t1_buf[0] <= cntr_t1_gamma;		//FIFO first element
//			cntr_t2_buf[0] <= cntr_t2_gamma;
			cntr_amp_buf[0] <= cntr_max_gamma;
			cntr_isd_buf[0] <= 1'd1;
			cntr_type_buf[0] <= 1'd1;			//gamma
			cntr_t0 <= 32'd1;
			cntr_gamma_saveflag <= 1'b0;
			mes_in_FIFO <= mes_in_FIFO+16'd1;
		end
		else mes_lost <= mes_lost + 32'd1;
		
		if ( (cntr_alpha_saveflag == 1'd0) && (((!cntr_sign_alpha) && (out_a_fifo[out_a_time_shift] >= cntr_thresh_alpha)) || ((cntr_sign_alpha) && (out_a_fifo[out_a_time_shift] <= cntr_thresh_alpha))) ) begin		//alpha adc over threshold
			if (cntr_alpha_ongoing == 1'd1) begin															
				if ( ((!cntr_sign_alpha) && (out_a_fifo[out_a_time_shift] > cntr_max_alpha)) || ((cntr_sign_alpha) && (out_a_fifo[out_a_time_shift] < cntr_max_alpha)) ) begin					//new maxval found
					cntr_max_alpha <= out_a_fifo[out_a_time_shift];
//					cntr_t2_alpha <= 32'd0;
				end
//				else	cntr_t2_alpha <= cntr_t2_alpha + 32'd1;
				
				cntr_t1_alpha <= cntr_t1_alpha + 32'd1;
			end
			else begin						//if its not ongoing we start it
				cntr_alpha_ongoing <= 1'd1;
				cntr_t1_alpha <= 32'd0;
				cntr_max_alpha <= out_a_fifo[out_a_time_shift];
//				cntr_t2_alpha <= 32'd0;
			end
		end
		else if (cntr_alpha_ongoing == 1'd1) begin			//if it is ongoing but we went below threshold
			cntr_alpha_ongoing <= 1'd0;
			if (cntr_t1_alpha>=cntr_mintime_alpha) cntr_alpha_saveflag <= 1'b1;	//we save only if the peak is wide enough (else it might be noise or something else)
		end
		
		if ( (cntr_gamma_saveflag == 1'd0) && (((!cntr_sign_gamma) && (out_b_dela >= cntr_thresh_gamma)) || ((cntr_sign_gamma) && (out_b_dela <= cntr_thresh_gamma))) ) begin		//gamma adc over threshold
			if (cntr_gamma_ongoing == 1'd1) begin															
				if ( ((!cntr_sign_gamma) && (out_b_dela > cntr_max_gamma)) || ((cntr_sign_gamma) && (out_b_dela < cntr_max_gamma)) ) begin
					cntr_max_gamma <= out_b_dela;
//					cntr_t2_gamma <= 32'd0;
				end
//				else	cntr_t2_gamma <= cntr_t2_gamma + 32'd1;
				
				cntr_t1_gamma <= cntr_t1_gamma + 32'd1;
			end
			else begin			
				cntr_gamma_ongoing <= 1'd1;
				cntr_t1_gamma <= 32'd0;
				cntr_max_gamma <= out_b_dela;
//				cntr_t2_gamma <= 32'd0;
			end
		end
		else if (cntr_gamma_ongoing == 1'd1) begin	
			cntr_gamma_ongoing <= 1'd0;
			if (cntr_t1_gamma>=cntr_mintime_gamma) cntr_gamma_saveflag <= 1'b1;
		end
		
		if (max_mes_in_FIFO<mes_in_FIFO) max_mes_in_FIFO <= mes_in_FIFO;	//just to log the largest number of elements in FIFO at any point after reset
		
		for (i=0; i<`FIFO_size-1; i=i+1)					//here we shift the FIFO
			if ((cntr_isd_buf[i+1]==1'd0)&&(cntr_isd_buf[i]==1'd1))  begin
				cntr_t0_buf[i+1] <= cntr_t0_buf[i];
				cntr_t1_buf[i+1] <= cntr_t1_buf[i];
//				cntr_t2_buf[i+1] <= cntr_t2_buf[i];
				cntr_amp_buf[i+1] <= cntr_amp_buf[i];
				cntr_type_buf[i+1] <= cntr_type_buf[i];
				cntr_isd_buf[i+1] <= cntr_isd_buf[i];
				cntr_t0_buf[i] <= 32'd0;
				cntr_t1_buf[i] <= 32'd0;
//				cntr_t2_buf[i] <= 32'd0;
				cntr_amp_buf[i] <= 14'sd0;
				cntr_type_buf[i] <= 1'd0;
				cntr_isd_buf[i] <= 1'd0;
			end
			
				
	end
end



////------------------------------- System bus connection -----------------------------////


always @(posedge clk_i) begin
	if (rstn_i == 1'b0) begin
		cntr_thresh_alpha <= 14'sd8191;
		cntr_sign_alpha <= 1'd0;			//0 == rising edge trigger
		cntr_thresh_gamma <= 14'sd8191;
		cntr_sign_gamma <= 1'd0;
		cntr_mintime_alpha <= 32'd4294967295;
		cntr_mintime_gamma <= 32'd4294967295;
		out_a_time_shift <= 8'd0;
		reset_fifo <= 1'd0;
		
	end
	else begin
		if (sys_wen) begin
			casez (sys_addr[19:0])
				20'h0000	: begin 	
							cntr_thresh_alpha <= sys_wdata[13:0];
				        	  	cntr_sign_alpha <= sys_wdata[31];
				        	  end
				20'h0004	: begin	
							cntr_thresh_gamma <= sys_wdata[13:0];
				        	  	cntr_sign_gamma <= sys_wdata[31];	
				        	  end		
				20'h0008	: 	cntr_mintime_alpha <= sys_wdata[31:0];
				20'h000C	: 	cntr_mintime_gamma <= sys_wdata[31:0];
				
				20'h0010	: 	reset_fifo <= ~reset_fifo;
				//20'h0014	used
				//20'h0018	used
				20'h001C	: 	out_a_time_shift <= sys_wdata[7:0];	
			endcase

		end
	end
end

wire sys_en;
assign sys_en = sys_wen | sys_ren;


always @(posedge clk_i)
if (rstn_i == 1'b0) begin
   sys_err <= 1'b0 ;
   sys_ack <= 1'b0 ;
   mes_received <= 1'd0;
end else begin
   sys_err <= 1'b0 ;
	casez (sys_addr[19:0])
		20'h0000		: begin sys_ack <= sys_en;	sys_rdata <= {cntr_sign_alpha, {32-15{1'b0}}, cntr_thresh_alpha}; end
		20'h0004		: begin sys_ack <= sys_en;	sys_rdata <= {cntr_sign_gamma, {32-15{1'b0}}, cntr_thresh_gamma}; end
		20'h0008		: begin sys_ack <= sys_en;	sys_rdata <= cntr_mintime_alpha	; end
		20'h000C		: begin sys_ack <= sys_en;	sys_rdata <= cntr_mintime_gamma	; end

		//20'h0010	used
		20'h0014		: begin sys_ack <= sys_en;	sys_rdata <= mes_lost; end
		20'h0018		: begin sys_ack <= sys_en;	sys_rdata <= {max_mes_in_FIFO,mes_in_FIFO}; end
		20'h001C		: begin sys_ack <= sys_en;	sys_rdata <= {{32-8{1'b0}},		out_a_time_shift		}; end
		
		20'h0020		: begin 
						sys_ack <= sys_en;	
						sys_rdata <= {cntr_isd_buf[`FIFO_size-1],cntr_type_buf[`FIFO_size-1],cntr_amp_buf[`FIFO_size-1],{32-16{1'b0}}}; 
					  end
		20'h0024		: begin 
						sys_ack <= sys_en;	
						sys_rdata <= cntr_t0_buf[`FIFO_size-1]; 
					  end
		20'h0028		: begin 
						sys_ack <= sys_en;	
						sys_rdata <= cntr_t1_buf[`FIFO_size-1]; 
						if (sys_ren && (cntr_isd_buf[`FIFO_size-1]==1'd1)) mes_received <= ~mes_received; 	//last element has been read
					  end
//		20'h002C		: begin 
//						sys_ack <= sys_en;	
//						sys_rdata <= cntr_t2_buf[`FIFO_size-1]; 	//ALLWAYS read this buffer last as it will delete the FIFO element
//						if (sys_ren && (cntr_isd_buf[`FIFO_size-1]==1'd1)) mes_received <= ~mes_received; 	//last element has been read
//					  end
		
		

		default	:	begin sys_ack <= sys_en;	sys_rdata <=  32'h0;	end
	endcase
end

endmodule
