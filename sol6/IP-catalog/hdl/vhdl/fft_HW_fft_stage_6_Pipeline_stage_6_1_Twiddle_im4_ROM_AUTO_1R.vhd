-- ==============================================================
-- Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
-- Version: 2022.2
-- Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
-- ==============================================================
library ieee; 
use ieee.std_logic_1164.all; 
use ieee.std_logic_unsigned.all;

entity fft_HW_fft_stage_6_Pipeline_stage_6_1_Twiddle_im4_ROM_AUTO_1R is 
    generic(
             DataWidth     : integer := 32; 
             AddressWidth     : integer := 5; 
             AddressRange    : integer := 32
    ); 
    port (
 
          address0        : in std_logic_vector(AddressWidth-1 downto 0); 
          ce0             : in std_logic; 
          q0              : out std_logic_vector(DataWidth-1 downto 0);

          reset               : in std_logic;
          clk                 : in std_logic
    ); 
end entity; 


architecture rtl of fft_HW_fft_stage_6_Pipeline_stage_6_1_Twiddle_im4_ROM_AUTO_1R is 
 
signal address0_tmp : std_logic_vector(AddressWidth-1 downto 0); 

type mem_array is array (0 to AddressRange-1) of std_logic_vector (DataWidth-1 downto 0); 

signal mem0 : mem_array := (
    0 => "10000000000000000000000000000000", 1 => "10111101110010001011110100110110", 2 => "10111110010001111100010111000010", 3 => "10111110100101001010000000110010", 
    4 => "10111110110000111110111100010110", 5 => "10111110111100010101101011101010", 6 => "10111111000011100011100111011010", 7 => "10111111001000100110011110011010", 
    8 => "10111111001101010000010011110011", 9 => "10111111010001011110010000000011", 10 => "10111111010101001101101100110010", 11 => "10111111011000011100010110011000", 
    12 => "10111111011011001000001101011110", 13 => "10111111011101001111101000001011", 14 => "10111111011110110001010010111111", 15 => "10111111011111101100010001101101", 
    16 => "10111111100000000000000000000000", 17 => "10111111011111101100010001101101", 18 => "10111111011110110001010010111110", 19 => "10111111011101001111101000001010", 
    20 => "10111111011011001000001101011110", 21 => "10111111011000011100010110010111", 22 => "10111111010101001101101100110000", 23 => "10111111010001011110010000000100", 
    24 => "10111111001101010000010011110011", 25 => "10111111001000100110011110011001", 26 => "10111111000011100011100111011001", 27 => "10111110111100010101101011100110", 
    28 => "10111110110000111110111100010000", 29 => "10111110100101001010000000110011", 30 => "10111110010001111100010111000001", 31 => "10111101110010001011110100110000");



begin 

 
memory_access_guard_0: process (address0) 
begin
      address0_tmp <= address0;
--synthesis translate_off
      if (CONV_INTEGER(address0) > AddressRange-1) then
           address0_tmp <= (others => '0');
      else 
           address0_tmp <= address0;
      end if;
--synthesis translate_on
end process;

p_rom_access: process (clk)  
begin 
    if (clk'event and clk = '1') then
 
        if (ce0 = '1') then  
            q0 <= mem0(CONV_INTEGER(address0_tmp)); 
        end if;

end if;
end process;

end rtl;

