// function call handling
                    {
                    Token fn_token = expr[i];
                    Token token = expr[++i];
                    if(token.type!=TOKEN_OPAREN){
                        TOKENERROR(" Error: you probably skipped '()' when function call. Otherwise you are f@cked up. Got ")
                    }
                    token = expr[++i];
                    Func fn_to_call = functions[hash(fn_token.sv)%1024];
                    fn_to_call.body.variables=malloc(sizeof(Variables)*10);
                    for(int i = 0; i<10; i++){
                        fn_to_call.body.variables[i].varc = 0;
                    }
                    for(size_t j = 0; j<fn_to_call.argc; j++){
                        Variable var;
                        var.name     = fn_to_call.args[j].name;
                        var.type     = fn_to_call.args[j].type;
                        var.modifyer = fn_to_call.args[j].modifyer;
                        if(var.modifyer == MOD_ARRAY){
                            Variable src = get_var_by_name(token.sv, variables, depth);
                            if(src.modifyer!=MOD_ARRAY){
                                printloc(token.loc);
                                logf(" Error: expected array, got %s \n", TYPE_TO_STR[src.type]);
                                exit(1);
                            }
                            var.size = src.size;
                            var.ptr = malloc(get_type_size_in_bytes(var.type) * src.size);
                            copy_array(var, src);
                            token = expr[++i];
                            token = expr[++i];
                        } else { // if var not array
                            token = expr[++i];
                            Token *arg_expr_start = &expr[i];
                            int arg_exprc = 0;
                            while(token.type != TOKEN_COMMA){
                                if(token.type == TOKEN_CPAREN){
                                    TOKENERROR(" Error: uhhm, bruh. You forgor some arguments ")
                                }
                                token = expr[++i];
                                arg_exprc++;
                            }
                            token = expr[++i];
                            int64_t argument_value = evaluate_expr(arg_expr_start, arg_exprc, variables, depth);
                            var.ptr = malloc(get_type_size_in_bytes(var.type));
                            var_num_cast(&var, argument_value);
                        }

                        if(variables[depth].varc == 0){
                            fn_to_call.body.variables[1].variables = malloc(sizeof(Variable));
                        } else {
                            fn_to_call.body.variables[1].variables =
                                realloc(fn_to_call.body.variables[1].variables, sizeof(Variable)*(fn_to_call.body.variables[1].varc+1));
                        }
                        fn_to_call.body.variables[1].variables[j] = var;
                        fn_to_call.body.variables[1].varc++;
                        fn_to_call.body.depth = 1;
                    }
                    if(fn_to_call.name.data == NULL){
                        TOKENERROR(" Error: unknown directive ");
                    }
                    while(token.type != TOKEN_SEMICOLON){
                        token = expr[++i];
                    }
                    postfix[j].type = RPN_NUM;
                    postfix[j].numeric = evaluate_code_block(fn_to_call.body).value;
                    j++;
                    break;
                    //-function-call-handling-
                    }
