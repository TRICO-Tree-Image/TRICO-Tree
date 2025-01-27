`timescale 1ns / 1ps
//----------------------------------------------------------------------------//
// Organization: XXX
// Author: XXX
// 
// Create Date: 2024/11/03 16:09:50
// Module Name: ram_true_dual_port
// Target Devices: Xilinx_Alveo_U200
// Tool Versions: Vivado 2020.2
// Description: Declaration of the simple dual port RAM module
// 
// Dependencies: none
// 
// Version: v1.0
// Additional Comments:Read and write occur at the same address, the data read out is the newly written data.
// 
//----------------------------------------------------------------------------//

module INFER_SDPRAM 
#(
    parameter 	ADDR_WIDTH       =4                     ,
    parameter 	DATA_WIDTH       =4                     ,
    parameter 	DEPTH            =(2**ADDR_WIDTH)       ,
    parameter  INIT_VALUE       ='d0
)(    
    input             clk                               ,
    input             rst_n                             ,

    input      	      wr_en                             , 
    input      	      [ADDR_WIDTH-1:0] addr_a           ,             
    input  	          [DATA_WIDTH-1:0] data_a           ,

    input             re_en                             ,   
    input      	      [ADDR_WIDTH-1:0] addr_b           ,         
    output   reg      [DATA_WIDTH-1:0] data_b
    );
reg [DATA_WIDTH-1:0]    ram_data [DEPTH-1:0];  
integer i;

always @(posedge clk or negedge rst_n) begin
    if(!rst_n) begin    
        for(i = 0;i < DEPTH;i = i + 1) begin
            ram_data[i] = INIT_VALUE;
        end
    end
end

always @(posedge clk) begin
    if(wr_en) begin
        ram_data[addr_a] <= data_a;
    end
end

always @(posedge clk or negedge rst_n) begin
    if(!rst_n) begin       
        data_b <= 0;
    end
    else if(re_en) begin 
        data_b <= ((addr_b == addr_a)&wr_en) ? data_a : ram_data[addr_b];
    end
    else begin
        data_b <= data_b;
    end
end

endmodule