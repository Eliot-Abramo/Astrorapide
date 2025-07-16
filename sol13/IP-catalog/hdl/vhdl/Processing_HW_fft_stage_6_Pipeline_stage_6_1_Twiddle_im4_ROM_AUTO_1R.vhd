-- ==============================================================
-- Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
-- Version: 2022.2
-- Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
-- ==============================================================
library ieee; 
use ieee.std_logic_1164.all; 
use ieee.std_logic_unsigned.all;

entity Processing_HW_fft_stage_6_Pipeline_stage_6_1_Twiddle_im4_ROM_AUTO_1R is 
    generic(
             DataWidth     : integer := 23; 
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


architecture rtl of Processing_HW_fft_stage_6_Pipeline_stage_6_1_Twiddle_im4_ROM_AUTO_1R is 
 
signal address0_tmp : std_logic_vector(AddressWidth-1 downto 0); 

type mem_array is array (0 to AddressRange-1) of std_logic_vector (DataWidth-1 downto 0); 

signal mem0 : mem_array := (
    0 => "00000000000000000000000", 1 => "11110011011101000010110", 2 => "11100111000001110100011", 3 => "11011010110101111111001", 
    4 => "11001111000001000011101", 5 => "11000011101010010100010", 6 => "10111000111000110001001", 7 => "10101110110011000011001", 
    8 => "10100101011111011000011", 9 => "10011101000011011111111", 10 => "10010101100100100110011", 11 => "10001111000111010011010", 
    12 => "10001001101111100101000", 13 => "10000101100000101111101", 14 => "10000010011101011010000", 15 => "10000000100111011100100", 
    16 => "10000000000000000000000", 17 => "10000000100111011100100", 18 => "10000010011101011010000", 19 => "10000101100000101111101", 
    20 => "10001001101111100101000", 21 => "10001111000111010011010", 22 => "10010101100100100110011", 23 => "10011101000011011111111", 
    24 => "10100101011111011000011", 25 => "10101110110011000011001", 26 => "10111000111000110001001", 27 => "11000011101010010100011", 
    28 => "11001111000001000011110", 29 => "11011010110101111111001", 30 => "11100111000001110100011", 31 => "11110011011101000010110");



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

