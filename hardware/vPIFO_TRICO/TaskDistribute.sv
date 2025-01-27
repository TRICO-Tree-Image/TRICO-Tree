

module TaskDistribute 
#(
	parameter PTW    = 16,       // Payload data width
	parameter MTW    = 16,       // Meta data width
	parameter LEVEL  = 4,         // Sub-tree level ie RPU num
    parameter PLW             = 8, //packet length width
	parameter TREE_NUM = 4,
    localparam LEVEL_BITS = $clog2(LEVEL),
	localparam TREE_NUM_BITS = $clog2(TREE_NUM),
	localparam TaskFIFO_DATA_BITS = (PTW+MTW+PLW)+(TREE_NUM_BITS+TREE_NUM_BITS)+2,
	localparam TaskFIFO_DATA_PUSH_TaskFIFO_INVERT = {{1'b1}, {(TaskFIFO_DATA_BITS-1){1'b0}}},
	localparam TaskFIFO_DATA_POP_TaskFIFO_INVERT = {{1'b0}, {1'b1}, {(TaskFIFO_DATA_BITS-2){1'b0}}},
	localparam TaskFIFO_DATA_PP_TaskFIFO_INVERT = {{1'b1}, {1'b1}, {(TaskFIFO_DATA_BITS-2){1'b0}}}
)(
    // Clock and Reset
    input                           i_clk,         // I - Clock
    input                           i_arst_n,      // I - Active Low Async Reset
    // TaskFIFO
    output [LEVEL-1:0]              o_pop_TaskFIFO,
    // { push_bit, pop_bit, TreeId, PushData(or '0 when pop)}
    input [TaskFIFO_DATA_BITS-1:0]  i_TaskFIFO_data [0:LEVEL-1], 
    input [LEVEL-1:0]               i_TaskFIFO_empty,

    // output push pop task
    output [LEVEL-1:0]              o_rpu_push,
    output [LEVEL-1:0]              o_rpu_pop,
    output [TREE_NUM_BITS-1:0]      o_rpu_treeId [0:LEVEL-1],
    output [(PTW+MTW+PLW)-1:0]          o_rpu_push_data [0:LEVEL-1]
);

localparam  ST_IDLE     = 3'b000,
            ST_PUSH     = 3'b001,
            ST_POP      = 3'b011,
            ST_WB       = 3'b010,
            ST_PP       = 3'b100;

// {push_bit, pop_bit, TreeId, PushData(or '0 when pop)}
reg [TaskFIFO_DATA_BITS-1:0] TaskHead_last [0:LEVEL-1];
reg [TaskFIFO_DATA_BITS-1:0] TaskHead [0:LEVEL-1];
reg [TaskFIFO_DATA_BITS-1:0] TaskHead_after [0:LEVEL-1];
reg [LEVEL-1:0] pop_TaskFIFO_last;
reg [LEVEL-1:0] i_TaskFIFO_empty_last;

wire [1:0] TaskHead_type [0:LEVEL-1]; // [1] is push_bit, [0] is pop_bit
wire [1:0] TaskHead_type_after [0:LEVEL-1]; // [1] is push_bit, [0] is pop_bit
wire [TREE_NUM_BITS-1:0] TaskHead_push_treeId [0:LEVEL-1];
wire [TREE_NUM_BITS-1:0] TaskHead_pop_treeId [0:LEVEL-1];
wire [(PTW+MTW+PLW)-1:0] TaskHead_push_data [0:LEVEL-1];

reg [LEVEL-1:0] rpu_push, 
                rpu_push_nxt, 
                rpu_push_new, 
                rpu_push_new_nxt, 
                rpu_push_inherit, 
                rpu_push_inherit_nxt,
                rpu_push_inherit_mid_nxt;

reg [LEVEL-1:0] rpu_pop,
                rpu_pop_nxt,
                rpu_pop_new,
                rpu_pop_new_nxt,
                rpu_pop_inherit,
                rpu_pop_inherit_nxt,
                rpu_pop_inherit_mid_nxt;

reg [TREE_NUM_BITS-1:0] rpu_treeId [0:LEVEL-1];
reg [(PTW+MTW+PLW)-1:0] rpu_push_data [0:LEVEL-1];

// RPU state
reg [2:0] rpu_state [0:LEVEL-1];
reg [2:0] rpu_state_mid [0:LEVEL-1];
reg [LEVEL_BITS-1:0] rpu_level [0:LEVEL-1];
reg [LEVEL_BITS-1:0] rpu_level_nxt [0:LEVEL-1];
wire [LEVEL-1:0] rpu_ready_nxt ;

// ============================ drive rpu push pop =======================================

// rpu_push and rpu_pop
for (genvar i = 0; i < LEVEL; i++) begin
    assign rpu_push_nxt[i] = rpu_push_new_nxt[i] | rpu_push_inherit_nxt[i];
    assign rpu_pop_nxt[i] = rpu_pop_new_nxt[i] | rpu_pop_inherit_nxt[i];
end

for (genvar i = 0; i < LEVEL; i++) begin
    always @ (posedge i_clk or negedge i_arst_n) begin
        if (!i_arst_n) begin
            rpu_push[i] <= '0;
            rpu_pop[i] <= '0;
        end else begin
            rpu_push[i] <= rpu_push_nxt[i];
            rpu_pop[i] <= rpu_pop_nxt[i];
        end
    end
end


// rpu_push_inherit_nxt and rpu_pop_inherit_nxt
for (genvar i = 1; i < LEVEL; i++) begin
    assign rpu_push_inherit_nxt[i] = rpu_push[i-1] && (rpu_level[i-1] < LEVEL-1);
    assign rpu_pop_inherit_nxt[i] = rpu_pop[i-1] && (rpu_level[i-1] < LEVEL-1);
end
assign rpu_push_inherit_nxt[0] = rpu_push[LEVEL-1] && (rpu_level[LEVEL-1] < LEVEL-1);
assign rpu_pop_inherit_nxt[0] = rpu_pop[LEVEL-1] && (rpu_level[LEVEL-1] < LEVEL-1);

for (genvar i = 0; i < LEVEL; i++) begin
    always @ (posedge i_clk or negedge i_arst_n) begin
        if (!i_arst_n) begin
            rpu_push_inherit[i] <= '0;
            rpu_pop_inherit[i] <= '0;
        end else begin
            rpu_push_inherit[i] <= rpu_push_inherit_nxt[i];
            rpu_pop_inherit[i] <= rpu_pop_inherit_nxt[i];
        end
    end
end

// ============================ drive rpu state and level =======================================

// rpu_level_nxt
// if push_inherit or pop_inherit, level_nxt is level + 1
// else is push_new or pop_new, level_nxt is 0
for (genvar i = 1; i < LEVEL; i++) begin
    assign rpu_level_nxt[i] = (rpu_push_inherit_nxt[i] || rpu_pop_inherit_nxt[i] || rpu_pop_inherit[i]) ? (rpu_level[i-1]+1) : '0;
end
assign rpu_level_nxt[0] = (rpu_push_inherit_nxt[0] || rpu_pop_inherit_nxt[0] || rpu_pop_inherit[0]) ? (rpu_level[LEVEL-1]+1) : '0;

// rpu_state and rpu_level
for (genvar i = 0; i < LEVEL; i++) begin
    always @ (posedge i_clk or negedge i_arst_n) begin
        if (!i_arst_n) begin
            rpu_state[i] <= ST_IDLE;
            rpu_level[i] <= '0;
        end else begin
            case (rpu_state[i])
                ST_IDLE: begin
                    case ({rpu_push_nxt[i], rpu_pop_nxt[i]})
                        2'b00: begin // Not allow concurrent read and write
                            rpu_state[i] <= ST_IDLE;
                            rpu_level[i] <= '0;	 
                        end
                        2'b01: begin // pop
                            rpu_state[i] <= ST_POP;
                            rpu_level[i] <= rpu_level_nxt[i];	 
                        end
                        2'b10: begin // push
                            rpu_state[i] <= ST_PUSH;
                            rpu_level[i] <= rpu_level_nxt[i];
                        end
                        2'b11: begin
                            rpu_state[i] <= ST_PP;
                            rpu_level[i] <= rpu_level_nxt[i];	
                        end
                    endcase
                end

                ST_PUSH: begin       	
                    case ({rpu_push_nxt[i], rpu_pop_nxt[i]})
                        2'b00: begin 
                            rpu_state[i] <= ST_IDLE;
                            rpu_level[i] <= '0;
                        end
                        2'b01: begin
                            rpu_state[i] <= ST_POP;
                            rpu_level[i] <= rpu_level_nxt[i];
                        end
                        2'b10: begin 
                            rpu_state[i] <= ST_PUSH;
                            rpu_level[i] <= rpu_level_nxt[i];
                        end
                        2'b11: begin
                            rpu_state[i] <= ST_PP;
                            rpu_level[i] <= rpu_level_nxt[i];	
                        end
                    endcase   
                end   

                ST_POP: begin
                    rpu_state[i] <= ST_WB;
                    rpu_level[i] <= rpu_level[i];
                end		
                ST_PP: begin
                    rpu_state[i] <= ST_WB;
                    rpu_level[i] <= rpu_level[i];
                end
                
                ST_WB: begin		       
                    case ({rpu_push_nxt[i], rpu_pop_nxt[i]})
                        2'b00: begin 
                            rpu_state[i] <= ST_IDLE;
                            rpu_level[i] <= '0;
                        end
                        2'b01: begin
                            rpu_state[i] <= ST_POP;
                            rpu_level[i] <= rpu_level_nxt[i];
                        end
                        2'b10: begin 
                            rpu_state[i] <= ST_PUSH;
                            rpu_level[i] <= rpu_level_nxt[i];
                        end
                        2'b11: begin 
                            rpu_state[i] <= ST_PP;
                            rpu_level[i] <= rpu_level_nxt[i];
                        end
                    endcase
                end			
            endcase
        end	  
    end
end

// rpu_level_mid is rpu_level_nxt
    always_comb begin
        for (int i = 0; i < LEVEL; i++) begin
            if (!i_arst_n) begin
                rpu_state_mid[i] = ST_IDLE;
            end else begin
                case (rpu_state[i])
                    ST_IDLE: begin
                        case ({rpu_push_inherit_nxt[i], rpu_pop_inherit_nxt[i]})
                            2'b00: begin // Not allow concurrent read and write
                                rpu_state_mid[i] = ST_IDLE;
                            end
                            2'b01: begin // pop
                                rpu_state_mid[i] = ST_POP;
                            end
                            2'b10: begin // push
                                rpu_state_mid[i] = ST_PUSH;
                            end
                            2'b11: begin
                                rpu_state_mid[i] = ST_PP;
                            end
                        endcase
                    end

                    ST_PUSH: begin       	
                        case ({rpu_push_inherit_nxt[i], rpu_pop_inherit_nxt[i]})
                            2'b00: begin 
                                rpu_state_mid[i] = ST_IDLE;
                            end
                            2'b01: begin
                                rpu_state_mid[i] = ST_POP;
                            end
                            2'b10: begin 
                                rpu_state_mid[i] = ST_PUSH;
                            end
                            2'b11: begin
                                rpu_state_mid[i] = ST_PP;
                            end
                        endcase   
                    end   

                    ST_POP: begin
                        rpu_state_mid[i] = ST_WB;
                    end		
                    ST_PP: begin
                        rpu_state_mid[i] = ST_WB;
                    end		
                    ST_WB: begin		       
                        case ({rpu_push_inherit_nxt[i], rpu_pop_inherit_nxt[i]})
                            2'b00,
                            2'b11: begin 
                                rpu_state_mid[i] = ST_IDLE;
                            end
                            2'b01: begin
                                rpu_state_mid[i] = ST_POP;
                            end
                            2'b10: begin 
                                rpu_state_mid[i] = ST_PUSH;
                            end
                            2'b11: begin
                                rpu_state_mid[i] = ST_PP;
                            end
                        endcase
                    end			
                endcase
            end
        end
    end

// ============================ end drive rpu state and level =======================================

// ============================ drive TaskHead =======================================

for (genvar i = 0; i < LEVEL; i++) begin    
    assign TaskHead[i] = (!i_arst_n) ? '0 // reset
    : (!pop_TaskFIFO_last[i]) ?  TaskHead_last[i] // not pop
    : i_TaskFIFO_empty_last[i] ? {1'b0, i_TaskFIFO_data[i]} : {1'b1, i_TaskFIFO_data[i]}; // if empty
end

for (genvar i = 0; i < LEVEL; i++) begin
    always_ff @( posedge i_clk ) begin
        pop_TaskFIFO_last[i] <= o_pop_TaskFIFO[i];
        i_TaskFIFO_empty_last[i] <= i_TaskFIFO_empty[i];
        TaskHead_last[i] <= TaskHead_after[i];
    end
end

for (genvar i = 0; i < LEVEL; i++) begin    
    assign TaskHead_type_after[i] = TaskHead_after[i][TaskFIFO_DATA_BITS-1:TaskFIFO_DATA_BITS-2];
    assign TaskHead_type[i] = TaskHead[i][TaskFIFO_DATA_BITS-1:TaskFIFO_DATA_BITS-2];
    assign TaskHead_push_treeId[i] = TaskHead[i][TaskFIFO_DATA_BITS-3:(PTW+MTW+PLW)+TREE_NUM_BITS];
    assign TaskHead_pop_treeId[i] = TaskHead[i][(PTW+MTW+PLW)+TREE_NUM_BITS-1:(PTW+MTW+PLW)];
    assign TaskHead_push_data[i] = TaskHead[i][(PTW+MTW+PLW)-1:0];
end

for (genvar i = 0; i < LEVEL; i++) begin
    assign o_pop_TaskFIFO[i] = (TaskHead_type_after[i] == '0) && (!i_TaskFIFO_empty[i]);
end

//update_mkxue
for (genvar i = 0; i < LEVEL; i++) begin 
    assign TaskHead_after[i] = (rpu_push_new_nxt[i]&&rpu_pop_new_nxt[i]) ? (TaskHead[i] ^ TaskFIFO_DATA_PP_TaskFIFO_INVERT):
                                rpu_push_new_nxt[i] ? (TaskHead[i] ^ TaskFIFO_DATA_PUSH_TaskFIFO_INVERT) :
                                rpu_pop_new_nxt[i] ? (TaskHead[i] ^ TaskFIFO_DATA_POP_TaskFIFO_INVERT) : TaskHead[i];
end

// ============================ end drive TaskHead =======================================

// ============================ drive rpu_push_new and rpu_pop_new =======================================

for (genvar i = 0; i < LEVEL; i++) begin 
    assign rpu_ready_nxt[i] = (rpu_state_mid[i] == ST_IDLE) 
                            || (rpu_state_mid[i] != ST_POP && rpu_level_nxt[i] == LEVEL-1)
                            || (rpu_state_mid[i] != ST_PP && rpu_level_nxt[i] == LEVEL-1)
                            ;// ST_PUSH or ST_WB
end

for (genvar i = 1; i < LEVEL-1; i++) begin
    assign rpu_push_new_nxt[i] = (rpu_state_mid[i] != ST_IDLE) ? 1'b0 : TaskHead_type[i][1];
    
    assign rpu_pop_new_nxt[i] = (rpu_state_mid[i] != ST_IDLE || !rpu_ready_nxt[i-1]/*rpu_state_mid[i-1] != ST_IDLE*/ || (rpu_pop_new_nxt[i-1] | rpu_push_new_nxt[i-1])) ? 1'b0 : (TaskHead_type[i][0] && !TaskHead_type[i][1]);
end

assign rpu_push_new_nxt[0] = (rpu_state_mid[0] != ST_IDLE) ? 1'b0 : TaskHead_type[0][1];
assign rpu_pop_new_nxt[0] = (rpu_state_mid[0] != ST_IDLE || !rpu_ready_nxt[LEVEL-1]/*rpu_state_mid[0] != ST_IDLE*/) ? 1'b0 : (TaskHead_type[0][0] && !TaskHead_type[0][1]);

assign rpu_push_new_nxt[LEVEL-1] = (rpu_state_mid[LEVEL-1] != ST_IDLE) ? 1'b0 
: (rpu_pop_new_nxt[0] == 1'b1) ? 1'b0 
: TaskHead_type[LEVEL-1][1];

assign rpu_pop_new_nxt[LEVEL-1] = (rpu_state_mid[LEVEL-1] != ST_IDLE) ? 1'b0 
: (rpu_pop_new_nxt[0] == 1'b1) ? 1'b0 
: (!rpu_ready_nxt[LEVEL-2] /*rpu_state_mid[LEVEL-1] != ST_IDLE*/ || (rpu_pop_new_nxt[LEVEL-2] | rpu_push_new_nxt[LEVEL-2])) ? 1'b0 
: (TaskHead_type[LEVEL-1][0] && !TaskHead_type[LEVEL-1][1]);

always_ff @( posedge i_clk ) begin
    rpu_pop_new <= rpu_pop_new_nxt;
    rpu_push_new <= rpu_push_new_nxt;
end

// ============================ end drive rpu_push_new and rpu_pop_new =======================================


// ============================ output assign =======================================

wire [(PTW+MTW+PLW)-1:0] TaskHead_PushData_1;
assign TaskHead_PushData_1 = TaskHead_push_data[1];

for (genvar i = 0; i < LEVEL; i++) begin
    always_ff @( posedge i_clk ) begin
        rpu_push_data[i] <= TaskHead_push_data[i];
        rpu_treeId[i] <= rpu_push_new_nxt[i] ? TaskHead_push_treeId[i] :
                        rpu_pop_new_nxt[i] ? TaskHead_pop_treeId[i] : '0;
    end
end

assign o_rpu_push = rpu_push_new;
assign o_rpu_pop = rpu_pop_new;

for (genvar i = 0; i < LEVEL; i++) begin
    assign o_rpu_push_data[i] = rpu_push_data[i];
    assign o_rpu_treeId[i] = rpu_treeId[i];
end

// ============================ end output assign =======================================
endmodule