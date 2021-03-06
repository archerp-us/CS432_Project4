/**
 * @file p4-codegen.c
 * @brief Compiler phase 4: code generation
 * @authors Philip Archer and Xinzhe He
 */
#include "p4-codegen.h"

/**
 * @brief State/data for the code generator visitor
 */
typedef struct CodeGenData
{
    /**
     * @brief Reference to the epilogue jump label for the current function
     */
    Operand current_epilogue_jump_label;

    /* add any new desired state information (and clean it up in CodeGenData_free) */
} CodeGenData;

/**
 * @brief Allocate memory for code gen data
 * 
 * @returns Pointer to allocated structure
 */
CodeGenData* CodeGenData_new ()
{
    CodeGenData* data = (CodeGenData*)calloc(1, sizeof(CodeGenData));
    CHECK_MALLOC_PTR(data);
    data->current_epilogue_jump_label = empty_operand();
    return data;
}

/**
 * @brief Deallocate memory for code gen data
 * 
 * @param data Pointer to the structure to be deallocated
 */
void CodeGenData_free (CodeGenData* data)
{
    /* free everything in data that is allocated on the heap */

    /* free "data" itself */
    free(data);
}

/**
 * @brief Macro for more convenient access to the error list inside a @c visitor
 * data structure
 */
#define DATA ((CodeGenData*)visitor->data)

/**
 * @brief Fills a register with the base address of a variable.
 * 
 * @param node AST node to emit code into (if needed)
 * @param variable Desired variable
 * @returns Virtual register that contains the base address
 */
Operand var_base (ASTNode* node, Symbol* variable)
{
    Operand reg = empty_operand();
    switch (variable->location) {
        case STATIC_VAR:
            reg = virtual_register();
            ASTNode_emit_insn(node,
                    ILOCInsn_new_2op(LOAD_I, int_const(variable->offset), reg));
            break;
        case STACK_PARAM:
        case STACK_LOCAL:
            reg = base_register();
            break;
        default:
            break;
    }
    return reg;
}

/**
 * @brief Calculates the offset of a scalar variable reference and fills a register with that offset.
 * 
 * @param node AST node to emit code into (if needed)
 * @param variable Desired variable
 * @returns Virtual register that contains the base address
 */
Operand var_offset (ASTNode* node, Symbol* variable)
{
    Operand op = empty_operand();
    switch (variable->location) {
        case STATIC_VAR:    op = int_const(0); break;
        case STACK_PARAM:
        case STACK_LOCAL:   op = int_const(variable->offset);
        default:
            break;
    }
    return op;
}

#ifndef SKIP_IN_DOXYGEN

/*
 * Macros for more convenient instruction generation
 */

#define EMIT0OP(FORM)             ASTNode_emit_insn(node, ILOCInsn_new_0op(FORM))
#define EMIT1OP(FORM,OP1)         ASTNode_emit_insn(node, ILOCInsn_new_1op(FORM,OP1))
#define EMIT2OP(FORM,OP1,OP2)     ASTNode_emit_insn(node, ILOCInsn_new_2op(FORM,OP1,OP2))
#define EMIT3OP(FORM,OP1,OP2,OP3) ASTNode_emit_insn(node, ILOCInsn_new_3op(FORM,OP1,OP2,OP3))

void CodeGenVisitor_gen_program (NodeVisitor* visitor, ASTNode* node)
{
    /*
     * make sure "code" attribute exists at the program level even if there are
     * no functions (although this shouldn't happen if static analysis is run
     * first); also, don't include a print function here because there's not
     * really any need to re-print all the functions in the program node *
     */
    ASTNode_set_attribute(node, "code", InsnList_new(), (Destructor)InsnList_free);

    /* copy code from each function */
    FOR_EACH(ASTNode*, func, node->program.functions) {
        ASTNode_copy_code(node, func);
    }
}

void CodeGenVisitor_previsit_funcdecl (NodeVisitor* visitor, ASTNode* node)
{
    /* generate a label reference for the epilogue that can be used while
     * generating the rest of the function (e.g., to be used when generating
     * code for a "return" statement) */
    DATA->current_epilogue_jump_label = anonymous_label();
}

void CodeGenVisitor_gen_funcdecl (NodeVisitor* visitor, ASTNode* node)
{
    /* every function begins with the corresponding call label */
    EMIT1OP(LABEL, call_label(node->funcdecl.name));

    /* BOILERPLATE: TODO: implement prologue */

    /* copy code from body */
    ASTNode_copy_code(node, node->funcdecl.body);

    EMIT1OP(LABEL, DATA->current_epilogue_jump_label);
    /* BOILERPLATE: TODO: implement epilogue */
    EMIT0OP(RETURN);
}

// function to generate code for literals
void CodeGenVisitor_gen_literal (NodeVisitor* visitor, ASTNode* node)
{
	Operand reg = virtual_register();

	EMIT2OP(LOAD_I, int_const(node->literal.integer), reg);
	ASTNode_set_temp_reg(node, reg);
}

// function to generate code for return
void CodeGenVisitor_gen_return (NodeVisitor* visitor, ASTNode* node)
{
	ASTNode_copy_code(node, node->funcreturn.value);
	EMIT2OP(I2I, ASTNode_get_temp_reg(node->funcreturn.value), return_register());
}

// function to generate code for blocks
void CodeGenVisitor_gen_block (NodeVisitor* visitor, ASTNode* node)
{
	FOR_EACH(ASTNode*, statement, node->block.statements)
	{
		ASTNode_copy_code(node, statement);
	}
}

// function to generate code for binary operators
void CodeGenVisitor_gen_binaryop (NodeVisitor* visitor, ASTNode* node)
{
    ASTNode_copy_code(node, node->binaryop.left);
    ASTNode_copy_code(node, node->binaryop.right);
	
	Operand reg = virtual_register();
    Operand left_reg = ASTNode_get_temp_reg(node->binaryop.left);
	Operand right_reg = ASTNode_get_temp_reg(node->binaryop.right);
	
	//special case for ADD_I and MULT_I
	if (node->binaryop.right->type == LITERAL)
	{
		if (node->binaryop.operator == ADDOP)
		{
			EMIT3OP(ADD_I, left_reg, int_const(node->binaryop.right->literal.integer), reg);
		}
		else if (node->binaryop.operator == MULOP)
		{
			EMIT3OP(MULT_I, left_reg, int_const(node->binaryop.right->literal.integer), reg);
		}
	}
	//regular ADD and MULT
	else {
		if (node->binaryop.operator == ADDOP)
		{
			EMIT3OP(ADD, left_reg, right_reg, reg);
		}
		else if (node->binaryop.operator == MULOP)
		{
			EMIT3OP(MULT, left_reg, right_reg, reg);
		}
	}

	//check for binary operator
	//integers
	if (node->binaryop.operator == SUBOP)
	{
		EMIT3OP(SUB, left_reg, right_reg, reg);
	}
	else if (node->binaryop.operator == DIVOP)
	{
		EMIT3OP(DIV, left_reg, right_reg, reg);
	}
	//booleans
	else if (node->binaryop.operator == ANDOP)
	{
		EMIT3OP(AND, left_reg, right_reg, reg);
	}
	else if (node->binaryop.operator == OROP)
	{
		EMIT3OP(OR, left_reg, right_reg, reg);
	}
	//comparisons
	else if (node->binaryop.operator == LTOP)
	{
		EMIT3OP(CMP_LT, left_reg, right_reg, reg);
	}
	else if (node->binaryop.operator == LEOP)
	{
		EMIT3OP(CMP_LE, left_reg, right_reg, reg);
	}
	else if (node->binaryop.operator == EQOP)
	{
		EMIT3OP(CMP_EQ, left_reg, right_reg, reg);
	}
	else if (node->binaryop.operator == GEOP)
	{
		EMIT3OP(CMP_GE, left_reg, right_reg, reg);
	}
	else if (node->binaryop.operator == GTOP)
	{
		EMIT3OP(CMP_GT, left_reg, right_reg, reg);
	}
	else if (node->binaryop.operator == NEQOP)
	{
		EMIT3OP(CMP_NE, left_reg, right_reg, reg);
	}

    ASTNode_set_temp_reg(node, reg);
}

// function to generate code for unary operators
void CodeGenVisitor_gen_unaryop (NodeVisitor* visitor, ASTNode* node)
{
	ASTNode_copy_code(node, node->unaryop.child);
	
	Operand reg = virtual_register();
    Operand child_reg = ASTNode_get_temp_reg(node->unaryop.child);
	
	//check for unary commands, NOT and NEG
	if (node->unaryop.operator == NEGOP)
	{
		EMIT2OP(NEG, child_reg, reg);
	}
	else if (node->unaryop.operator == NOTOP)
	{
		EMIT2OP(NOT, child_reg, reg);
	}
	
	ASTNode_set_temp_reg(node, reg);
}

// function to generate code for assignments
void CodeGenVisitor_gen_assignment (NodeVisitor* visitor, ASTNode* node)
{
	ASTNode_copy_code(node, node->assignment.location);
	ASTNode_copy_code(node, node->assignment.value);
	
    Operand location_reg = ASTNode_get_temp_reg(node->assignment.location);
	Operand value_reg = ASTNode_get_temp_reg(node->assignment.value);
	
	EMIT2OP(STORE, value_reg, location_reg);
}

#endif
InsnList* generate_code (ASTNode* tree)
{
	if (tree == NULL)
	{
		return InsnList_new();
	}
	
    InsnList* iloc = InsnList_new();


    NodeVisitor* v = NodeVisitor_new();
    v->data = CodeGenData_new();
    v->dtor = (Destructor)CodeGenData_free;
    v->postvisit_program     = CodeGenVisitor_gen_program;
    v->previsit_funcdecl     = CodeGenVisitor_previsit_funcdecl;
    v->postvisit_funcdecl    = CodeGenVisitor_gen_funcdecl;
	v->postvisit_literal	 = CodeGenVisitor_gen_literal;
	v->postvisit_return		 = CodeGenVisitor_gen_return;
	v->postvisit_block		 = CodeGenVisitor_gen_block;
	v->postvisit_binaryop    = CodeGenVisitor_gen_binaryop;
	v->postvisit_unaryop     = CodeGenVisitor_gen_unaryop;
	//v->postvisit_assignment  = CodeGenVisitor_gen_assignment;
	//v->postvisit_location    = CodeGenVisitor_gen_location;

    /* generate code into AST attributes */
    NodeVisitor_traverse_and_free(v, tree);

    /* copy generated code into new list (the AST may be deallocated before
     * the ILOC code is needed) */
    FOR_EACH(ILOCInsn*, i, (InsnList*)ASTNode_get_attribute(tree, "code")) {
        InsnList_add(iloc, ILOCInsn_copy(i));
    }
    return iloc;
}
