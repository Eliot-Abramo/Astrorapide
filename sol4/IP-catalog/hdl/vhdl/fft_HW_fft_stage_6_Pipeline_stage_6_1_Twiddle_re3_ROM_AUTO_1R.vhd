-- ==============================================================
-- Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
-- Version: 2022.2
-- Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
-- ==============================================================
library ieee; 
use ieee.std_logic_1164.all; 
use ieee.std_logic_unsigned.all;

entity fft_HW_fft_stage_6_Pipeline_stage_6_1_Twiddle_re3_ROM_AUTO_1R is 
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


architecture rtl of fft_HW_fft_stage_6_Pipeline_stage_6_1_Twiddle_re3_ROM_AUTO_1R is 
 
signal address0_tmp : std_logic_vector(AddressWidth-1 downto 0); 

type mem_array is array (0 to AddressRange-1) of std_logic_vector (DataWidth-1 downto 0); 

signal mem0 : mem_array := (
    0 => "00111111100000000000000000000000", 1 => "00111111011111101100010001101101", 2 => "00111111011110110001010010111110", 3 => "00111111011101001111101000001011", 
    4 => "00111111011011001000001101011110", 5 => "00111111011000011100010110010111", 6 => "00111111010101001101101100110001", 7 => "00111111010001011110010000000011", 
    8 => "00111111001101010000010011110011", 9 => "00111111001000100110011110011001", 10 => "00111111000011100011100111011001", 11 => "00111110111100010101101011100111", 
    12 => "00111110110000111110111100010101", 13 => "00111110100101001010000000110000", 14 => "00111110010001111100010110111100", 15 => "00111101110010001011110100110101", 
    16 => "10110011001110111011110100101110", 17 => "10111101110010001011110101000001", 18 => "10111110010001111100010111000010", 19 => "10111110100101001010000000110011", 
    20 => "10111110110000111110111100011000", 21 => "10111110111100010101101011101101", 22 => "10111111000011100011100111011100", 23 => "10111111001000100110011110011001", 
    24 => "10111111001101010000010011110011", 25 => "10111111010001011110010000000100", 26 => "10111111010101001101101100110010", 27 => "10111111011000011100010110011001", 
    28 => "10111111011011001000001101100000", 29 => "10111111011101001111101000001011", 30 => "10111111011110110001010010111111", 31 => "10111111011111101100010001101101");



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

