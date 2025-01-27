import gurobipy as gp

# Create a new model
def load_balancing_solve(capacity_demand_list, core_capacity_list, core_frequency_list, core_legal_capacity_list, path_list):
    # Define variables
    c = capacity_demand_list
    C = core_capacity_list
    f = core_frequency_list
    s = core_legal_capacity_list
    P = path_list
    N = len(c)      # Number of PIFO nodes
    T = len(s[0])   # Number of possible capacity
    M = len(C)      # Number of PIFO cores
    K = len(P)      # Number of PIFO cores

    model = gp.Model("PIFO_Load_Balancing")
    # set time limit 120s
    model.setParam('TimeLimit', 120)
    
    # clear the model
    model.reset()

    X = model.addVars(N, M, T, vtype=gp.GRB.BINARY, name="X")
    F = model.addVar(name="F")
    cprime = model.addVars(N, vtype=gp.GRB.INTEGER, name="cprime")

    # Set objective function
    model.setObjective(F, gp.GRB.MAXIMIZE)
    # Add allocation constraint
    # \forall n\in \{1,\dots, N\}, \sum_{m=1}^M \sum_{t=1}^T x_{nmt} = 1
    for n in range(N):
        model.addConstr(gp.quicksum(X[n, m, t] for m in range(M) for t in range(T)) == 1)

    # Add capacity constraint
    # \forall m\in \{1,\dots,M\}, \sum_{n=1}^N \sum_{t=1}^T x_{nmt}\cdot s_{mt}\leq C_m
    for m in range(M):
        model.addConstr(gp.quicksum(X[n, m, t] * s[m][t] for n in range(N) for t in range(T)) <= C[m])

    # Add occupation constraint
    # c_n' = \sum_{m=1}^M \sum_{t=1}^T x_{nmt} s_{mt}
    for n in range(N):
        model.addConstr(cprime[n] == gp.quicksum(X[n, m, t] * s[m][t] for m in range(M) for t in range(T)))

    # Add occupation constraint
    # \forall n\in \{1,\dots,N\}, c_n\leq c_n'
    for n in range(N):
        model.addConstr(c[n] <= cprime[n])

    # Add frequency constraint
    # \forall k\in \{1,\dots,K\},m\in \{1,\dots,M\}
    # \sum_{i=1}^{i=\text{len}(P_k)}\sum_{t=1}^Tx_{p_{ki}mt}F\leq f_m
    for k in range(K):
        for m in range(M):
            model.addConstr(gp.quicksum(X[P[k][i], m, t] * F for i in range(len(P[k])) for t in range(T)) <= f[m])

    # Optimize the model, if can't find the optimal solution, return 0
    model.optimize()

    if model.status == gp.GRB.TIME_LIMIT:
        if model.solCount > 0:
            model.Params.SolutionNumber = model.solCount - 1
            # return the objective value of a feasible solution
            return model.objVal
        else:
            return 0
        
    elif model.status != gp.GRB.OPTIMAL:
        model.terminate()
        return 0
    
    # Print the optimal solution
    if model.status == gp.GRB.OPTIMAL:
        for v in model.getVars():
            if v.x > 0:
                print(f"{v.varName} = {int(v.x)}")

    print(f"Optimal objective value: {model.objVal}")
    model.terminate()
    return model.objVal