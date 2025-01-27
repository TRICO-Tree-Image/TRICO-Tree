`timescale 1ns / 10ps

// LEVEL == SRAM num, LEVEL == RPU num

module IO_PORT#(
    parameter PTW             = 16, // Payload data width
    parameter MTW             = 32, // Metdata width should not less than TREE_NUM_BITS, cause tree_id will be placed in MTW
    parameter CTW             = 10, // Counter width
    parameter LEVEL           = 2, // Sub-tree level
    parameter TREE_NUM        = 2,
    parameter FIFO_SIZE       = 8,
    parameter PLW             = 12, //packet length width
    parameter CDW             = 16, //credit length width

    localparam FIFO_WIDTH       = $clog2(FIFO_SIZE),
    localparam LEVEL_BITS       = $clog2(LEVEL),
    localparam TREE_NUM_BITS    = $clog2(TREE_NUM),
    localparam ROOT_TREE_ID     = 0,
    localparam ROOT_RPU_ID      = 0
)(
    input                            i_clk,
    input                            i_arst_n,
    input [TREE_NUM_BITS-1:0]        i_push_tree_id,
    input [PTW-1:0]                  i_push_priority,
    input                            i_push,
    input [(MTW+PTW+PLW)-1:0]        i_push_data,
    input                            i_pop,
    output [TREE_NUM_BITS-1:0]       o_pop_tree_id,
    output [(MTW+PTW+PLW)-1:0]       o_pop_data,
    output                           o_pop_out,
    output                           o_task_fifo_full,
    //
    output [TREE_NUM_BITS-1:0]pifo_i_push_tree_id_o,
    output push_o                                             ,
    output [(MTW+PTW+PLW)-1:0]push_data_o                     ,
    output popx_o                                             ,
    output [TREE_NUM_BITS-1:0]pifo_i_pop_tree_id_o ,
    input [TREE_NUM_BITS-1:0]pifo_o_tree_id_i      ,
    input [(MTW+PTW+PLW)-1:0]pop_data_i                       ,
    input is_level0_pop_i                                     ,
    input task_fifo_full_i
);

wire [TREE_NUM_BITS-1:0] pifo_i_push_tree_id [0:LEVEL-1]; 
wire [LEVEL-1:0] push;                                    
wire [(MTW+PTW+PLW)-1:0] push_data [0:LEVEL-1];           
wire [LEVEL-1:0] popx;                                    
wire [TREE_NUM_BITS-1:0] pifo_i_pop_tree_id [0:LEVEL-1];  

reg  [TREE_NUM_BITS-1:0] r_pifo_i_push_tree_id;
reg  r_push; 
reg  [(MTW+PTW+PLW)-1:0] r_push_data;
reg  [LEVEL-1:0] r_popx;
reg  [TREE_NUM_BITS-1:0] r_pifo_i_pop_tree_id;

reg  [TREE_NUM_BITS-1:0] pifo_o_tree_id [0:LEVEL-1] ;         
reg  [(MTW+PTW+PLW)-1:0] pop_data [0:LEVEL-1]       ;               
reg  [LEVEL-1:0] is_level0_pop                      ;                              
reg  [LEVEL-1:0] task_fifo_full                     ;                      

assign pifo_i_push_tree_id_o    =   r_pifo_i_push_tree_id;
assign push_o                   =   r_push               ;
assign push_data_o              =   r_push_data          ;
assign popx_o                   =   r_popx               ;
assign pifo_i_pop_tree_id_o     =   r_pifo_i_pop_tree_id ;

    for (genvar i = 0; i < LEVEL; i++) begin
        assign pifo_o_tree_id[i] = (pifo_o_tree_id_i==i) ? pifo_o_tree_id_i:'0;
        assign pop_data[i] = (pifo_o_tree_id_i==i)? pop_data_i:'0;
        assign is_level0_pop[i] = (pifo_o_tree_id_i==i)? is_level0_pop_i:'0;
        assign task_fifo_full[i] = (pifo_o_tree_id_i==i)? task_fifo_full_i:'0;
    end
    
    always_comb begin
        r_pifo_i_push_tree_id = '1;
        r_push = '1;
        r_push_data = '1;
        r_popx = '1;
        r_pifo_i_pop_tree_id = '1;
        for(int i=0; i<LEVEL; ++i)begin
            if(pifo_i_push_tree_id[i] != '1)
                r_pifo_i_push_tree_id = pifo_i_push_tree_id[i];
            if(push[i] != '1)
                r_push = push[i];
            if(push_data[i] != '1)
                r_push_data = push_data[i];
            if(popx[i] != '1)
                r_popx = popx[i];
            if(pifo_i_pop_tree_id[i] != '1)
                r_pifo_i_pop_tree_id = pifo_i_pop_tree_id[i];
        end
    end

    TASK_GENERATOR #(
    .PTW       (PTW),// Payload data width
    .MTW       (MTW),// Metdata width should not less than TREE_NUM_BITS, cause tree_id will be placed in MTW
    .CTW       (CTW),// Counter width
    .LEVEL     (LEVEL),// Sub-tree level
    .TREE_NUM  (TREE_NUM),
    .FIFO_SIZE (FIFO_SIZE),
    .PLW       (PLW),//packet length width
    .CDW       (CDW)//credit length width
)u_TASK_GENERATOR (
        .i_clk              (i_clk          ),
        .i_arst_n           (i_arst_n       ),
        .i_push_tree_id     (i_push_tree_id),
        .i_push_priority    (i_push_priority),
        .i_push             (i_push),
        .i_push_data        (i_push_data),
        .i_pop              (i_pop),
        .o_pop_tree_id      (o_pop_tree_id),
        .o_pop_data         (o_pop_data),
        .o_pop_out          (o_pop_out),
        .o_task_fifo_full   (o_task_f),
        //
        .pifo_i_push_tree_id(pifo_i_push_tree_id),
        .push               (push               ),
        .push_data          (push_data          ),
        .popx               (popx               ),
        .pifo_i_pop_tree_id (pifo_i_pop_tree_id ),
        .pifo_o_tree_id     (pifo_o_tree_id     ),
        .pop_data           (pop_data           ),
        .is_level0_pop      (is_level0_pop      ),
        .task_fifo_full     (task_fifo_full     )
    );

endmodule