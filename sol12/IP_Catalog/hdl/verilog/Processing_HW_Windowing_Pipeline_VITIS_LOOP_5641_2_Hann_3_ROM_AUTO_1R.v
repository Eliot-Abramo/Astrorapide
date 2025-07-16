// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
// Version: 2022.2
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// ==============================================================
`timescale 1 ns / 1 ps
module Processing_HW_Windowing_Pipeline_VITIS_LOOP_5641_2_Hann_3_ROM_AUTO_1R (
    address0, ce0, q0, 
    address1, ce1, q1, 
    address2, ce2, q2, 
    address3, ce3, q3, 
    reset, clk);

parameter DataWidth = 32;
parameter AddressWidth = 10;
parameter AddressRange = 820;
 
input[AddressWidth-1:0] address0;
input ce0;
output reg[DataWidth-1:0] q0;
 
input[AddressWidth-1:0] address1;
input ce1;
output reg[DataWidth-1:0] q1;
 
input[AddressWidth-1:0] address2;
input ce2;
output reg[DataWidth-1:0] q2;
 
input[AddressWidth-1:0] address3;
input ce3;
output reg[DataWidth-1:0] q3;

input reset;
input clk;

 
reg [DataWidth-1:0] rom0[0:AddressRange-1];
 
reg [DataWidth-1:0] rom1[0:AddressRange-1];


initial begin
     
    $readmemh("./Processing_HW_Windowing_Pipeline_VITIS_LOOP_5641_2_Hann_3_ROM_AUTO_1R.dat", rom0); 
    $readmemh("./Processing_HW_Windowing_Pipeline_VITIS_LOOP_5641_2_Hann_3_ROM_AUTO_1R.dat", rom1);
end

  
always @(posedge clk) 
begin 
    if (ce0) 
    begin
        q0 <= rom0[address0];
    end
end
  
always @(posedge clk) 
begin 
    if (ce1) 
    begin
        q1 <= rom0[address1];
    end
end
  
always @(posedge clk) 
begin 
    if (ce2) 
    begin
        q2 <= rom1[address2];
    end
end
  
always @(posedge clk) 
begin 
    if (ce3) 
    begin
        q3 <= rom1[address3];
    end
end


endmodule

