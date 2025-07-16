-- ==============================================================
-- Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
-- Version: 2022.2
-- Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
-- ==============================================================
library ieee; 
use ieee.std_logic_1164.all; 
use ieee.std_logic_unsigned.all;

entity Processing_HW_fft_stage_5_Pipeline_stage_5_1_Twiddle_re1_ROM_AUTO_1R is 
    generic(
             DataWidth     : integer := 24; 
             AddressWidth     : integer := 4; 
             AddressRange    : integer := 16
    ); 
    port (
 
          address0        : in std_logic_vector(AddressWidth-1 downto 0); 
          ce0             : in std_logic; 
          q0              : out std_logic_vector(DataWidth-1 downto 0);

          reset               : in std_logic;
          clk                 : in std_logic
    ); 
end entity; 


architecture rtl of Processing_HW_fft_stage_5_Pipeline_stage_5_1_Twiddle_re1_ROM_AUTO_1R is 
 
signal address0_tmp : std_logic_vector(AddressWidth-1 downto 0); 

type mem_array is array (0 to AddressRange-1) of std_logic_vector (DataWidth-1 downto 0); 

signal mem0 : mem_array := (
    0 => "010000000000000000000000", 1 => "001111101100010100101111", 2 => "001110110010000011010111", 3 => "001101010011011011001100", 
    4 => "001011010100000100111100", 5 => "001000111000111001110110", 6 => "000110000111110111100010", 7 => "000011000111110001011011", 
    8 => "111111111111111111111111", 9 => "111100111000001110100011", 10 => "111001111000001000011100", 11 => "110111000111000110001000", 
    12 => "110100101011111011000011", 13 => "110010101100100100110011", 14 => "110001001101111100101000", 15 => "110000010011101011010000");



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

