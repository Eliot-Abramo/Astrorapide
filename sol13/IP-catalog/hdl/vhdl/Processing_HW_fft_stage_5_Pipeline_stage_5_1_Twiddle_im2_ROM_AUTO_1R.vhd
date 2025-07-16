-- ==============================================================
-- Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
-- Version: 2022.2
-- Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
-- ==============================================================
library ieee; 
use ieee.std_logic_1164.all; 
use ieee.std_logic_unsigned.all;

entity Processing_HW_fft_stage_5_Pipeline_stage_5_1_Twiddle_im2_ROM_AUTO_1R is 
    generic(
             DataWidth     : integer := 23; 
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


architecture rtl of Processing_HW_fft_stage_5_Pipeline_stage_5_1_Twiddle_im2_ROM_AUTO_1R is 
 
signal address0_tmp : std_logic_vector(AddressWidth-1 downto 0); 

type mem_array is array (0 to AddressRange-1) of std_logic_vector (DataWidth-1 downto 0); 

signal mem0 : mem_array := (
    0 => "00000000000000000000000", 1 => "11100111000001110100011", 2 => "11001111000001000011101", 3 => "10111000111000110001001", 
    4 => "10100101011111011000011", 5 => "10010101100100100110011", 6 => "10001001101111100101000", 7 => "10000010011101011010000", 
    8 => "10000000000000000000000", 9 => "10000010011101011010000", 10 => "10001001101111100101000", 11 => "10010101100100100110011", 
    12 => "10100101011111011000011", 13 => "10111000111000110001001", 14 => "11001111000001000011110", 15 => "11100111000001110100011");



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

