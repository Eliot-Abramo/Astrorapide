   
    parameter PROC_NUM = 14;
    parameter ST_IDLE = 3'b000;
    parameter ST_FILTER_FAKE = 3'b001;
    parameter ST_DL_DETECTED = 3'b010;
    parameter ST_DL_REPORT = 3'b100;
   

    reg [2:0] CS_fsm;
    reg [2:0] NS_fsm;
    reg [PROC_NUM - 1:0] dl_detect_reg;
    reg [PROC_NUM - 1:0] dl_done_reg;
    reg [PROC_NUM - 1:0] origin_reg;
    reg [PROC_NUM - 1:0] dl_in_vec_reg;
    reg [31:0] dl_keep_cnt;
    integer i;
    integer fp;

    // FSM State machine
    always @ (negedge dl_reset or posedge dl_clock) begin
        if (~dl_reset) begin
            CS_fsm <= ST_IDLE;
        end
        else begin
            CS_fsm <= NS_fsm;
        end
    end
    always @ (CS_fsm or dl_in_vec or dl_detect_reg or dl_done_reg or dl_in_vec or origin_reg or dl_keep_cnt) begin
        case (CS_fsm)
            ST_IDLE : begin
                if (|dl_in_vec) begin
                    NS_fsm = ST_FILTER_FAKE;
                end
                else begin
                    NS_fsm = ST_IDLE;
                end
            end
            ST_FILTER_FAKE: begin
                if (dl_keep_cnt >= 32'd1000) begin
                    NS_fsm = ST_DL_DETECTED;
                end
                else if (dl_detect_reg != (dl_detect_reg & dl_in_vec)) begin
                    NS_fsm = ST_IDLE;
                end
                else begin
                    NS_fsm = ST_FILTER_FAKE;
                end
            end
            ST_DL_DETECTED: begin
                // has unreported deadlock cycle
                if (dl_detect_reg != dl_done_reg) begin
                    NS_fsm = ST_DL_REPORT;
                end
                else begin
                    NS_fsm = ST_DL_DETECTED;
                end
            end
            ST_DL_REPORT: begin
                if (|(dl_in_vec & origin_reg)) begin
                    NS_fsm = ST_DL_DETECTED;
                end
                else begin
                    NS_fsm = ST_DL_REPORT;
                end
            end
            default: NS_fsm = ST_IDLE;
        endcase
    end

    // dl_detect_reg record the procs that first detect deadlock
    always @ (negedge dl_reset or posedge dl_clock) begin
        if (~dl_reset) begin
            dl_detect_reg <= 'b0;
        end
        else begin
            if (CS_fsm == ST_IDLE) begin
                dl_detect_reg <= dl_in_vec;
            end
        end
    end

    always @ (negedge dl_reset or posedge dl_clock) begin
        if (~dl_reset) begin
            dl_keep_cnt <= 32'h0;
        end
        else begin
            if (CS_fsm == ST_FILTER_FAKE && (dl_detect_reg == (dl_detect_reg & dl_in_vec))) begin
                dl_keep_cnt <= dl_keep_cnt + 32'h1;
            end
            else if (CS_fsm == ST_FILTER_FAKE && (dl_detect_reg != (dl_detect_reg & dl_in_vec))) begin
                dl_keep_cnt <= 32'h0;
            end
        end
    end

    // dl_detect_out keeps in high after deadlock detected
    assign dl_detect_out = (|dl_detect_reg) && (CS_fsm == ST_DL_DETECTED || CS_fsm == ST_DL_REPORT);

    // dl_done_reg record the cycles has been reported
    always @ (negedge dl_reset or posedge dl_clock) begin
        if (~dl_reset) begin
            dl_done_reg <= 'b0;
        end
        else begin
            if ((CS_fsm == ST_DL_REPORT) && (|(dl_in_vec & dl_detect_reg) == 'b1)) begin
                dl_done_reg <= dl_done_reg | dl_in_vec;
            end
        end
    end

    // clear token once a cycle is done
    assign token_clear = (CS_fsm == ST_DL_REPORT) ? ((|(dl_in_vec & origin_reg)) ? 'b1 : 'b0) : 'b0;

    // origin_reg record the current cycle start id
    always @ (negedge dl_reset or posedge dl_clock) begin
        if (~dl_reset) begin
            origin_reg <= 'b0;
        end
        else begin
            if (CS_fsm == ST_DL_DETECTED) begin
                origin_reg <= origin;
            end
        end
    end
   
    // origin will be valid for only one cycle
    wire [PROC_NUM*PROC_NUM - 1:0] origin_tmp;
    assign origin_tmp[PROC_NUM - 1:0] = (dl_detect_reg[0] & ~dl_done_reg[0]) ? 'b1 : 'b0;
    genvar j;
    generate
    for(j = 1;j < PROC_NUM;j = j + 1) begin: F1
        assign origin_tmp[j*PROC_NUM +: PROC_NUM] = (dl_detect_reg[j] & ~dl_done_reg[j]) ? ('b1 << j) : origin_tmp[(j - 1)*PROC_NUM +: PROC_NUM];
    end
    endgenerate
    always @ (CS_fsm or origin_tmp) begin
        if (CS_fsm == ST_DL_DETECTED) begin
            origin = origin_tmp[(PROC_NUM - 1)*PROC_NUM +: PROC_NUM];
        end
        else begin
            origin = 'b0;
        end
    end

    
    // dl_in_vec_reg record the current cycle dl_in_vec
    always @ (negedge dl_reset or posedge dl_clock) begin
        if (~dl_reset) begin
            dl_in_vec_reg <= 'b0;
        end
        else begin
            if (CS_fsm == ST_DL_DETECTED) begin
                dl_in_vec_reg <= origin;
            end
            else if (CS_fsm == ST_DL_REPORT) begin
                dl_in_vec_reg <= dl_in_vec;
            end
        end
    end
    
    // find_df_deadlock to report the deadlock
    always @ (negedge dl_reset or posedge dl_clock) begin
        if (~dl_reset) begin
            find_df_deadlock <= 1'b0;
        end
        else begin
            if (CS_fsm == ST_DL_DETECTED && dl_detect_reg == dl_done_reg) begin
                find_df_deadlock <= 1'b1;
            end
            else if (CS_fsm == ST_IDLE) begin
                find_df_deadlock <= 1'b0;
            end
        end
    end
    
    // get the first valid proc index in dl vector
    function integer proc_index(input [PROC_NUM - 1:0] dl_vec);
        begin
            proc_index = 0;
            for (i = 0; i < PROC_NUM; i = i + 1) begin
                if (dl_vec[i]) begin
                    proc_index = i;
                end
            end
        end
    endfunction

    // get the proc path based on dl vector
    function [600:0] proc_path(input [PROC_NUM - 1:0] dl_vec);
        integer index;
        begin
            index = proc_index(dl_vec);
            case (index)
                0 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process_U0";
                end
                1 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0";
                end
                2 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0";
                end
                3 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0";
                end
                4 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0";
                end
                5 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0";
                end
                6 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0";
                end
                7 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0";
                end
                8 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0";
                end
                9 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0";
                end
                10 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0";
                end
                11 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0";
                end
                12 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0";
                end
                13 : begin
                    proc_path = "Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.normaling_U0";
                end
                default : begin
                    proc_path = "unknown";
                end
            endcase
        end
    endfunction

    // print the headlines of deadlock detection
    task print_dl_head;
        begin
            $display("\n//////////////////////////////////////////////////////////////////////////////");
            $display("// ERROR!!! DEADLOCK DETECTED at %0t ns! SIMULATION WILL BE STOPPED! //", $time);
            $display("//////////////////////////////////////////////////////////////////////////////");
            fp = $fopen("deadlock_db.dat", "w");
        end
    endtask

    // print the start of a cycle
    task print_cycle_start(input reg [600:0] proc_path, input integer cycle_id);
        begin
            $display("/////////////////////////");
            $display("// Dependence cycle %0d:", cycle_id);
            $display("// (1): Process: %0s", proc_path);
            $fdisplay(fp, "Dependence_Cycle_ID %0d", cycle_id);
            $fdisplay(fp, "Dependence_Process_ID 1");
            $fdisplay(fp, "Dependence_Process_path %0s", proc_path);
        end
    endtask

    // print the end of deadlock detection
    task print_dl_end(input integer num, input integer record_time);
        begin
            $display("////////////////////////////////////////////////////////////////////////");
            $display("// Totally %0d cycles detected!", num);
            $display("////////////////////////////////////////////////////////////////////////");
            $display("// ERROR!!! DEADLOCK DETECTED at %0t ns! SIMULATION WILL BE STOPPED! //", record_time);
            $display("//////////////////////////////////////////////////////////////////////////////");
            $fdisplay(fp, "Dependence_Cycle_Number %0d", num);
            $fclose(fp);
        end
    endtask

    // print one proc component in the cycle
    task print_cycle_proc_comp(input reg [600:0] proc_path, input integer cycle_comp_id);
        begin
            $display("// (%0d): Process: %0s", cycle_comp_id, proc_path);
            $fdisplay(fp, "Dependence_Process_ID %0d", cycle_comp_id);
            $fdisplay(fp, "Dependence_Process_path %0s", proc_path);
        end
    endtask

    // print one channel component in the cycle
    task print_cycle_chan_comp(input [PROC_NUM - 1:0] dl_vec1, input [PROC_NUM - 1:0] dl_vec2);
        reg [696:0] chan_path;
        integer index1;
        integer index2;
        begin
            index1 = proc_index(dl_vec1);
            index2 = proc_index(dl_vec2);
            case (index1)
                0 : begin
                    case(index2)
                    1: begin
                        if (~grp_Top_DataFlox_fu_236.process_U0.grp_process_Pipeline_VITIS_LOOP_169_3_fu_122.D0_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.D0_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D0_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.D0_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D0_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.process_U0.num_c47_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c47_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c47_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c47_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c47_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c47_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c47_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (grp_Top_DataFlox_fu_236.ap_sync_process_U0_ap_ready & grp_Top_DataFlox_fu_236.process_U0.ap_idle & ~grp_Top_DataFlox_fu_236.ap_sync_process2_U0_ap_ready) begin
                            $display("//      Blocked by input sync logic with process : 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                        end
                    end
                    13: begin
                        if (grp_Top_DataFlox_fu_236.ap_sync_process_U0_ap_ready & grp_Top_DataFlox_fu_236.process_U0.ap_idle & ~grp_Top_DataFlox_fu_236.ap_sync_normaling_U0_ap_ready) begin
                            $display("//      Blocked by input sync logic with process : 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.normaling_U0'");
                        end
                    end
                    endcase
                end
                1 : begin
                    case(index2)
                    0: begin
                        if (~grp_Top_DataFlox_fu_236.process2_U0.grp_Windowing_fu_98.grp_Windowing_Pipeline_VITIS_LOOP_5637_1_fu_46.D0_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.D0_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D0_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.D0_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D0_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.process2_U0.num_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c47_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c47_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c47_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c47_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c47_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c47_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (grp_Top_DataFlox_fu_236.ap_sync_process2_U0_ap_ready & grp_Top_DataFlox_fu_236.process2_U0.ap_idle & ~grp_Top_DataFlox_fu_236.ap_sync_process_U0_ap_ready) begin
                            $display("//      Blocked by input sync logic with process : 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process_U0'");
                        end
                    end
                    2: begin
                        if (~grp_Top_DataFlox_fu_236.process2_U0.grp_Windowing_fu_98.grp_Windowing_Pipeline_VITIS_LOOP_5641_2_fu_56.D1_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.D1_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D1_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D1_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.D1_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D1_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D1_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.process2_U0.num_c46_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c46_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c46_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c46_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c46_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c46_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c46_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_load_nfft_value_U0_U.if_full_n & grp_Top_DataFlox_fu_236.process2_U0.ap_start & ~grp_Top_DataFlox_fu_236.process2_U0.real_start & (trans_in_cnt_0 == trans_out_cnt_0) & ~grp_Top_DataFlox_fu_236.start_for_load_nfft_value_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_load_nfft_value_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0',");
                        end
                    end
                    13: begin
                        if (grp_Top_DataFlox_fu_236.ap_sync_process2_U0_ap_ready & grp_Top_DataFlox_fu_236.process2_U0.ap_idle & ~grp_Top_DataFlox_fu_236.ap_sync_normaling_U0_ap_ready) begin
                            $display("//      Blocked by input sync logic with process : 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.normaling_U0'");
                        end
                    end
                    endcase
                end
                2 : begin
                    case(index2)
                    1: begin
                        if (~grp_Top_DataFlox_fu_236.load_nfft_value_U0.grp_load_nfft_value_Pipeline_VITIS_LOOP_216_2_fu_82.D1_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.D1_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D1_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D1_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.D1_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D1_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.D1_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.load_nfft_value_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c46_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c46_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c46_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c46_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c46_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c46_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_load_nfft_value_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.load_nfft_value_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_load_nfft_value_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_load_nfft_value_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0',");
                        end
                    end
                    3: begin
                        if (~grp_Top_DataFlox_fu_236.load_nfft_value_U0.grp_load_nfft_value_Pipeline_VITIS_LOOP_220_3_fu_92.S0_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S0_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S0_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S0_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S0_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.load_nfft_value_U0.num_c45_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c45_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c45_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c45_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c45_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c45_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c45_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_1_U0_U.if_full_n & grp_Top_DataFlox_fu_236.load_nfft_value_U0.ap_start & ~grp_Top_DataFlox_fu_236.load_nfft_value_U0.real_start & (trans_in_cnt_1 == trans_out_cnt_1) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_1_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_1_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0',");
                        end
                    end
                    endcase
                end
                3 : begin
                    case(index2)
                    2: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_1_U0.grp_fft_stage_1_Pipeline_stage_1_counter_stage_1_total_fu_56.S0_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S0_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S0_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S0_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S0_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_1_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c45_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c45_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c45_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c45_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c45_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c45_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_1_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_1_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_1_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_1_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.load_nfft_value_U0',");
                        end
                    end
                    4: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_1_U0.grp_fft_stage_1_Pipeline_stage_1_counter_stage_1_total_fu_56.S1_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S1_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S1_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S1_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S1_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S1_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S1_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_1_U0.num_c44_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c44_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c44_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c44_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c44_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c44_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c44_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_2_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_1_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_1_U0.real_start & (trans_in_cnt_2 == trans_out_cnt_2) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_2_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_2_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0',");
                        end
                    end
                    endcase
                end
                4 : begin
                    case(index2)
                    3: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_2_U0.grp_fft_stage_2_Pipeline_stage_2_0_fu_98.S1_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S1_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S1_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S1_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S1_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S1_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S1_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_2_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c44_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c44_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c44_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c44_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c44_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c44_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_2_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_2_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_2_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_2_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_1_U0',");
                        end
                    end
                    5: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_2_U0.grp_fft_stage_2_Pipeline_stage_2_1_fu_106.S2_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S2_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S2_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S2_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S2_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S2_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S2_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_2_U0.num_c43_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c43_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c43_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c43_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c43_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c43_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c43_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_3_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_2_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_2_U0.real_start & (trans_in_cnt_3 == trans_out_cnt_3) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_3_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_3_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0',");
                        end
                    end
                    endcase
                end
                5 : begin
                    case(index2)
                    4: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_3_U0.grp_fft_stage_3_Pipeline_stage_3_0_fu_98.S2_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S2_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S2_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S2_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S2_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S2_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S2_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_3_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c43_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c43_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c43_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c43_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c43_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c43_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_3_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_3_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_3_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_3_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_2_U0',");
                        end
                    end
                    6: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_3_U0.grp_fft_stage_3_Pipeline_stage_3_2_fu_120.S3_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S3_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S3_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S3_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S3_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S3_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S3_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_3_U0.num_c42_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c42_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c42_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c42_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c42_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c42_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c42_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_4_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_3_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_3_U0.real_start & (trans_in_cnt_4 == trans_out_cnt_4) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_4_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_4_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0',");
                        end
                    end
                    endcase
                end
                6 : begin
                    case(index2)
                    5: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_4_U0.grp_fft_stage_4_Pipeline_stage_4_0_fu_98.S3_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S3_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S3_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S3_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S3_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S3_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S3_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_4_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c42_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c42_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c42_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c42_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c42_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c42_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_4_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_4_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_4_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_4_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_3_U0',");
                        end
                    end
                    7: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_4_U0.grp_fft_stage_4_Pipeline_stage_4_1_fu_106.S4_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S4_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S4_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S4_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S4_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S4_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S4_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_4_U0.num_c41_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c41_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c41_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c41_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c41_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c41_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c41_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_5_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_4_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_4_U0.real_start & (trans_in_cnt_5 == trans_out_cnt_5) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_5_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_5_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0',");
                        end
                    end
                    endcase
                end
                7 : begin
                    case(index2)
                    6: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_5_U0.grp_fft_stage_5_Pipeline_stage_5_0_fu_98.S4_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S4_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S4_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S4_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S4_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S4_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S4_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_5_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c41_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c41_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c41_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c41_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c41_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c41_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_5_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_5_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_5_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_5_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_4_U0',");
                        end
                    end
                    8: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_5_U0.grp_fft_stage_5_Pipeline_stage_5_1_fu_106.S5_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S5_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S5_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S5_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S5_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S5_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S5_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_5_U0.num_c40_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c40_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c40_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c40_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c40_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c40_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c40_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_6_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_5_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_5_U0.real_start & (trans_in_cnt_6 == trans_out_cnt_6) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_6_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_6_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0',");
                        end
                    end
                    endcase
                end
                8 : begin
                    case(index2)
                    7: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_6_U0.grp_fft_stage_6_Pipeline_stage_6_0_fu_98.S5_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S5_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S5_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S5_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S5_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S5_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S5_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_6_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c40_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c40_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c40_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c40_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c40_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c40_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_6_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_6_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_6_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_6_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_5_U0',");
                        end
                    end
                    9: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_6_U0.grp_fft_stage_6_Pipeline_stage_6_1_fu_106.S6_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S6_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S6_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S6_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S6_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S6_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S6_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_6_U0.num_c39_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c39_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c39_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c39_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c39_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c39_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c39_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_7_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_6_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_6_U0.real_start & (trans_in_cnt_7 == trans_out_cnt_7) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_7_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_7_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0',");
                        end
                    end
                    endcase
                end
                9 : begin
                    case(index2)
                    8: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_7_U0.grp_fft_stage_7_Pipeline_stage_7_0_fu_98.S6_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S6_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S6_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S6_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S6_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S6_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S6_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_7_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c39_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c39_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c39_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c39_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c39_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c39_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_7_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_7_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_7_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_7_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_6_U0',");
                        end
                    end
                    10: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_7_U0.grp_fft_stage_7_Pipeline_stage_7_1_fu_106.S7_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S7_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S7_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S7_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S7_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S7_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S7_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_7_U0.num_c38_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c38_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c38_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c38_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c38_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c38_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c38_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_8_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_7_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_7_U0.real_start & (trans_in_cnt_8 == trans_out_cnt_8) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_8_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_8_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0',");
                        end
                    end
                    endcase
                end
                10 : begin
                    case(index2)
                    9: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_8_U0.grp_fft_stage_8_Pipeline_stage_8_0_fu_114.S7_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S7_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S7_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S7_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S7_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S7_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S7_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_8_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c38_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c38_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c38_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c38_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c38_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c38_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_8_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_8_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_8_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_8_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_7_U0',");
                        end
                    end
                    11: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_8_U0.grp_fft_stage_8_Pipeline_stage_8_1_fu_124.S8_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S8_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S8_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S8_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S8_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S8_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S8_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_8_U0.num_c37_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c37_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c37_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c37_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c37_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c37_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c37_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_9_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_8_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_8_U0.real_start & (trans_in_cnt_9 == trans_out_cnt_9) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_9_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_9_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0',");
                        end
                    end
                    endcase
                end
                11 : begin
                    case(index2)
                    10: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_9_U0.grp_fft_stage_9_Pipeline_stage_9_0_fu_114.S8_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S8_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S8_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S8_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S8_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S8_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S8_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_9_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c37_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c37_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c37_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c37_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c37_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c37_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_9_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_9_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_9_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_9_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_8_U0',");
                        end
                    end
                    12: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_9_U0.grp_fft_stage_9_Pipeline_stage_9_2_fu_140.S9_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S9_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S9_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S9_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S9_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S9_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S9_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_9_U0.num_c36_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c36_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c36_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c36_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c36_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c36_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c36_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_10_U0_U.if_full_n & grp_Top_DataFlox_fu_236.fft_stage_9_U0.ap_start & ~grp_Top_DataFlox_fu_236.fft_stage_9_U0.real_start & (trans_in_cnt_10 == trans_out_cnt_10) & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_10_U0_U.if_read) begin
                            $display("//      Blocked by full output start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_10_U0_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0',");
                        end
                    end
                    endcase
                end
                12 : begin
                    case(index2)
                    11: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_10_U0.grp_fft_stage_10_Pipeline_stage_10_0_fu_104.S9_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S9_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S9_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S9_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S9_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S9_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S9_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_10_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c36_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c36_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c36_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c36_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c36_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c36_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.start_for_fft_stage_10_U0_U.if_empty_n & grp_Top_DataFlox_fu_236.fft_stage_10_U0.ap_idle & ~grp_Top_DataFlox_fu_236.start_for_fft_stage_10_U0_U.if_write) begin
                            $display("//      Blocked by missing 'ap_start' from start propagation FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.start_for_fft_stage_10_U0_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_9_U0',");
                        end
                    end
                    13: begin
                        if (~grp_Top_DataFlox_fu_236.fft_stage_10_U0.grp_fft_stage_10_Pipeline_stage_10_2_fu_130.S10_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S10_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S10_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.normaling_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S10_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S10_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S10_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.normaling_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S10_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.fft_stage_10_U0.num_c_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.normaling_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.normaling_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                    end
                    endcase
                end
                13 : begin
                    case(index2)
                    12: begin
                        if (~grp_Top_DataFlox_fu_236.normaling_U0.grp_normaling_Pipeline_VITIS_LOOP_5650_1_VITIS_LOOP_5651_2_fu_44.S10_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.S10_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S10_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S10_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.S10_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S10_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.S10_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                        if (~grp_Top_DataFlox_fu_236.normaling_U0.numFFT_blk_n) begin
                            if (~grp_Top_DataFlox_fu_236.num_c_U.if_empty_n) begin
                                $display("//      Blocked by empty input FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c_U' written by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c_U");
                                $fdisplay(fp, "Dependence_Channel_status EMPTY");
                            end
                            else if (~grp_Top_DataFlox_fu_236.num_c_U.if_full_n) begin
                                $display("//      Blocked by full output FIFO 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c_U' read by process 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.fft_stage_10_U0'");
                                $fdisplay(fp, "Dependence_Channel_path Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.num_c_U");
                                $fdisplay(fp, "Dependence_Channel_status FULL");
                            end
                        end
                    end
                    0: begin
                        if (grp_Top_DataFlox_fu_236.ap_sync_normaling_U0_ap_ready & grp_Top_DataFlox_fu_236.normaling_U0.ap_idle & ~grp_Top_DataFlox_fu_236.ap_sync_process_U0_ap_ready) begin
                            $display("//      Blocked by input sync logic with process : 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process_U0'");
                        end
                    end
                    1: begin
                        if (grp_Top_DataFlox_fu_236.ap_sync_normaling_U0_ap_ready & grp_Top_DataFlox_fu_236.normaling_U0.ap_idle & ~grp_Top_DataFlox_fu_236.ap_sync_process2_U0_ap_ready) begin
                            $display("//      Blocked by input sync logic with process : 'Processing_HW_Processing_HW.grp_Top_DataFlox_fu_236.process2_U0'");
                        end
                    end
                    endcase
                end
            endcase
        end
    endtask

    // report
    initial begin : report_deadlock
        integer cycle_id;
        integer cycle_comp_id;
        integer record_time;
        wait (dl_reset == 1);
        cycle_id = 1;
        record_time = 0;
        while (1) begin
            @ (negedge dl_clock);
            case (CS_fsm)
                ST_DL_DETECTED: begin
                    cycle_comp_id = 2;
                    if (dl_detect_reg != dl_done_reg) begin
                        if (dl_done_reg == 'b0) begin
                            print_dl_head;
                            record_time = $time;
                        end
                        print_cycle_start(proc_path(origin), cycle_id);
                        cycle_id = cycle_id + 1;
                    end
                    else begin
                        print_dl_end((cycle_id - 1),record_time);
                        @(negedge dl_clock);
                        @(negedge dl_clock);
                        $finish;
                    end
                end
                ST_DL_REPORT: begin
                    if ((|(dl_in_vec)) & ~(|(dl_in_vec & origin_reg))) begin
                        print_cycle_chan_comp(dl_in_vec_reg, dl_in_vec);
                        print_cycle_proc_comp(proc_path(dl_in_vec), cycle_comp_id);
                        cycle_comp_id = cycle_comp_id + 1;
                    end
                    else begin
                        print_cycle_chan_comp(dl_in_vec_reg, dl_in_vec);
                    end
                end
            endcase
        end
    end
 
