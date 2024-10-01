#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define sxcscript_path "test/6.txt"
#define sxcscript_capacity (1 << 16)
#define sxcscript_buf_capacity (1 << 10)

enum bool {
    false = 0,
    true = 1,
};
enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
    sxcscript_kind_push,
    sxcscript_kind_label,
    sxcscript_kind_call,
    sxcscript_kind_jmp,
    sxcscript_kind_jze,
    sxcscript_kind_const_get,
    sxcscript_kind_const_set,
    sxcscript_kind_local_get,
    sxcscript_kind_local_set,
    sxcscript_kind_add,
    sxcscript_kind_sub,
    sxcscript_kind_mul,
    sxcscript_kind_div,
    sxcscript_kind_mod,
};
enum sxcscript_global {
    sxcscript_global_ip,
    sxcscript_global_sp,
    sxcscript_global_bp,
};
struct sxcscript_token {
    const char* data;
    int32_t size;
};
union sxcscript_node_val {
    int32_t literal;
    struct sxcscript_label* label;
    struct sxcscript_label* jmp;
    struct sxcscript_token* fn;
};
struct sxcscript_node {
    enum sxcscript_kind kind;
    struct sxcscript_token* token;
    union sxcscript_node_val val;
    struct sxcscript_node* prev;
    struct sxcscript_node* next;
};
struct sxcscript_label {
    union {
        struct sxcscript_label* jmp;
        struct sxcscript_node* fn;
    } key;
    int32_t arg_size;
    int32_t inst_i;
};
struct sxcscript_inst {
    enum sxcscript_kind kind;
    int32_t val;
};
struct sxcscript {
    int32_t mem[sxcscript_capacity];
    struct sxcscript_inst inst[sxcscript_capacity];
    struct sxcscript_token token[sxcscript_capacity];
    struct sxcscript_node node[sxcscript_capacity];
    struct sxcscript_label label[sxcscript_capacity];
    struct sxcscript_node* free;
    struct sxcscript_node* parsed;
    struct sxcscript_node* var;
    int32_t label_size;
};

uint64_t xorshift(uint64_t* x) {
    *x ^= *x << 13;
    *x ^= *x >> 7;
    *x ^= *x << 17;
    return *x;
}
enum bool sxcscript_token_eq(struct sxcscript_token* a, struct sxcscript_token* b) {
    if (a->size != b->size) {
        return false;
    }
    for (int i = 0; i < a->size; i++) {
        if (a->data[i] != b->data[i]) {
            return false;
        }
    }
    return true;
}
enum bool sxcscript_token_eq_str(struct sxcscript_token* a, const char* b) {
    for (int i = 0; i < a->size; i++) {
        if (a->data[i] != b[i] || b[i] == '\0') {
            return false;
        }
    }
    return true;
}
int32_t sxcscript_token_to_int32(struct sxcscript_token* token) {
    enum bool is_neg = token->data[0] == '-';
    int i = is_neg ? 1 : 0;
    int32_t ret = 0;
    for (; i < token->size; i++) {
        ret = (ret * 10) + token->data[i] - '0';
    }
    return is_neg ? -ret : ret;
}
void sxcscript_node_free(struct sxcscript_node** free, struct sxcscript_node* node) {
    node->prev = *free;
    (*free)->next = node;
    *free = node;
}
struct sxcscript_node* sxcscript_node_alloc(struct sxcscript_node** free) {
    struct sxcscript_node* node = *free;
    *free = (*free)->prev;
    *node = (struct sxcscript_node){.prev = NULL, .next = NULL};
    return node;
}
struct sxcscript_node* sxcscript_node_insert(struct sxcscript_node** free, struct sxcscript_node* next) {
    struct sxcscript_node* node = sxcscript_node_alloc(free);
    struct sxcscript_node* prev = next->prev;
    next->prev = node;
    *node = (struct sxcscript_node){.prev = prev, .next = next};
    if (prev != NULL) {
        prev->next = node;
    }
    return node;
}
struct sxcscript_node* sxcscript_node_find(struct sxcscript_node* src, struct sxcscript_node* this) {
    for (struct sxcscript_node* itr = src->prev; itr != NULL; itr = itr->prev) {
        if (itr->token == this->token) {
            return itr;
        }
    }
    for (struct sxcscript_node* itr = src->prev; itr != NULL; itr = itr->prev) {
        if (sxcscript_token_eq(itr->token, this->token)) {
            return itr;
        }
    }
    return NULL;
}
int32_t sxcscript_node_left(struct sxcscript_node* node) {
    int32_t ret = 0;
    for (struct sxcscript_node* itr = node; itr->prev != NULL; itr = itr->prev) {
        ret++;
    }
    return ret;
}
void sxcscript_node_init(struct sxcscript* sxcscript) {
    sxcscript->free = sxcscript->node;
    *(sxcscript->free) = (struct sxcscript_node){.prev = NULL, .next = NULL};
    for (int i = 1; i < sxcscript_capacity; i++) {
        sxcscript_node_free(&sxcscript->free, &sxcscript->node[i]);
    }
    sxcscript->parsed = sxcscript_node_alloc(&sxcscript->free);
    sxcscript->var = sxcscript_node_alloc(&sxcscript->free);
    sxcscript->label_size = 0;
}
void sxcscript_tokenize(const char* src, struct sxcscript_token* token) {
    struct sxcscript_token* token_itr = token;
    *token_itr = (struct sxcscript_token){src, 0};
    for (const char* src_itr = src; *src_itr != '\0'; src_itr++) {
        if (*src_itr == ' ' || *src_itr == '\n') {
            if (token_itr->size != 0) {
                token_itr++;
            }
        } else if (*src_itr == '(' || *src_itr == ')' || *src_itr == ',' || *src_itr == '.') {
            if (token_itr->size != 0) {
                token_itr++;
            }
            *(token_itr++) = (struct sxcscript_token){src_itr, 1};
        } else {
            if (token_itr->size == 0) {
                token_itr->data = src_itr;
            }
            token_itr->size++;
        }
    }
}
void sxcscript_parse_push(struct sxcscript_node** free, struct sxcscript_node* parsed, enum sxcscript_kind kind, struct sxcscript_token* token, union sxcscript_node_val val) {
    struct sxcscript_node* node = sxcscript_node_insert(free, parsed);
    *node = (struct sxcscript_node){.kind = kind, .token = token, .val = val, .prev = node->prev, .next = node->next};
}
void sxcscript_parse_expr(struct sxcscript_node** free, struct sxcscript_node* parsed, struct sxcscript_token** token_itr, struct sxcscript_label** label_itr) {
    struct sxcscript_token* token_this = *token_itr;
    struct sxcscript_node* node_this = parsed;
    if (sxcscript_token_eq_str(token_this, "(")) {
        (*token_itr)++;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_expr(free, parsed, token_itr, label_itr);
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if (sxcscript_token_eq_str(token_this, ".")) {
        (*token_itr)++;
        sxcscript_parse_expr(free, parsed, token_itr, label_itr);
    } else if (sxcscript_token_eq_str(token_this, "if")) {
        struct sxcscript_label* label_if = (*label_itr)++;
        struct sxcscript_label* label_else = (*label_itr)++;
        (*token_itr)++;
        sxcscript_parse_expr(free, parsed, token_itr, label_itr);
        sxcscript_parse_push(free, parsed, sxcscript_kind_jze, NULL, (union sxcscript_node_val){.jmp = label_if});
        sxcscript_parse_expr(free, parsed, token_itr, label_itr);
        if (sxcscript_token_eq_str(*token_itr, "else")) {
            (*token_itr)++;
            sxcscript_parse_push(free, parsed, sxcscript_kind_jmp, NULL, (union sxcscript_node_val){.jmp = label_else});
            sxcscript_parse_push(free, parsed, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label = label_if});
            sxcscript_parse_expr(free, parsed, token_itr, label_itr);
            sxcscript_parse_push(free, parsed, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label = label_else});
        } else {
            sxcscript_parse_push(free, parsed, sxcscript_kind_label, NULL, (union sxcscript_node_val){.label = label_if});
        }
    } else if (sxcscript_token_eq_str(*token_itr + 1, "(")) {
        (*token_itr)++;
        sxcscript_parse_expr(free, parsed, token_itr, label_itr);
        sxcscript_parse_push(free, parsed, sxcscript_kind_call, token_this, (union sxcscript_node_val){NULL});
    } else {
        sxcscript_parse_push(free, parsed, sxcscript_kind_push, token_this, (union sxcscript_node_val){NULL});
        (*token_itr)++;
    }
}
void sxcscript_parse(struct sxcscript* sxcscript) {
    struct sxcscript_token* token_itr = sxcscript->token;
    struct sxcscript_label* label_itr = sxcscript->label;
    sxcscript_parse_expr(&sxcscript->free, sxcscript->parsed, &token_itr, &label_itr);
}
void sxcscript_analyze_label(struct sxcscript* sxcscript, struct sxcscript_node* parsed_begin) {
    struct sxcscript_node* parsed_itr = parsed_begin;
    struct sxcscript_label* label_itr = sxcscript->label;
    for (; parsed_itr->kind != sxcscript_kind_null; parsed_itr = parsed_itr->next) {
        if (parsed_itr->kind != sxcscript_kind_label) {
            continue;
        }
        *(label_itr++) = (struct sxcscript_label){parsed_itr, 0};
    }
}
void sxcscript_analyze_inst(struct sxcscript* sxcscript, struct sxcscript_node* parsed_begin) {
    struct sxcscript_node* parsed_itr = parsed_begin;
    struct sxcscript_node* local[sxcscript_buf_capacity];
    for (; parsed_itr->kind != sxcscript_kind_null; parsed_itr = parsed_itr->next) {
        if (parsed_itr->kind == sxcscript_kind_push) {
            parsed_itr->kind = sxcscript_kind_const_get;
            if ('0' <= parsed_itr->token->data[0] && parsed_itr->token->data[0] <= '9' || parsed_itr->token->data[0] == '-') {
                parsed_itr->val.literal = sxcscript_token_to_int32(parsed_itr->token);
            } else {
                for (int i = 0; 1; i++) {
                    if (local[i] == NULL) {
                        local[i] = parsed_itr;
                        if (parsed_itr->val.literal == 0) {
                            parsed_itr->val.literal = i;
                        }
                        break;
                    }
                    if (sxcscript_token_eq(local[i]->token, parsed_itr->token)) {
                        parsed_itr->val.literal = local[i]->val.literal;
                        break;
                    }
                }
            }
        } else if (parsed_itr->kind == sxcscript_kind_call) {
            if (sxcscript_token_eq_str(parsed_itr->token, "local_get")) {
                parsed_itr->kind = sxcscript_kind_local_get;
            } else if (sxcscript_token_eq_str(parsed_itr->token, "local_set")) {
                parsed_itr->kind = sxcscript_kind_local_set;
            } else if (sxcscript_token_eq_str(parsed_itr->token, "add")) {
                parsed_itr->kind = sxcscript_kind_add;
            } else if (sxcscript_token_eq_str(parsed_itr->token, "sub")) {
                parsed_itr->kind = sxcscript_kind_sub;
            } else if (sxcscript_token_eq_str(parsed_itr->token, "mul")) {
                parsed_itr->kind = sxcscript_kind_mul;
            } else if (sxcscript_token_eq_str(parsed_itr->token, "div")) {
                parsed_itr->kind = sxcscript_kind_div;
            } else if (sxcscript_token_eq_str(parsed_itr->token, "mod")) {
                parsed_itr->kind = sxcscript_kind_mod;
            } else {
                for (int i = 0; 1; i++) {
                    if (sxcscript_token_eq(sxcscript->label[i].key.fn->token, parsed_itr->token)) {
                        parsed_itr->val.literal = i;
                        break;
                    }
                }
            }
        } else if (parsed_itr->kind == sxcscript_kind_label || parsed_itr->kind == sxcscript_kind_jmp || parsed_itr->kind == sxcscript_kind_jze) {
            for (int i = 0; 1; i++) {
                if (sxcscript->label[i].key.jmp == parsed_itr->val.literal) {
                    parsed_itr->val.literal = i;
                    break;
                }
            }
        }
    }
}
void sxcscript_analyze(struct sxcscript* sxcscript) {
    struct sxcscript_node* parsed_itr = sxcscript->parsed;
    struct sxcscript_node* label[sxcscript_buf_capacity];
    while (parsed_itr->prev != NULL) {
        parsed_itr = parsed_itr->prev;
    }
    sxcscript_analyze_label(sxcscript, parsed_itr);
    sxcscript_analyze_inst(sxcscript, parsed_itr);
}
void sxcscript_toinst(struct sxcscript* sxcscript) {
    struct sxcscript_node* parsed_itr = sxcscript->parsed;
    while (parsed_itr->prev != NULL) {
        parsed_itr = parsed_itr->prev;
    }
    for (struct sxcscript_inst* inst_itr = sxcscript->inst; parsed_itr->kind != sxcscript_kind_null; parsed_itr = parsed_itr->next) {
        if (parsed_itr->kind == sxcscript_kind_label) {
            sxcscript->label[parsed_itr->val.literal].inst_i = inst_itr - sxcscript->inst;
        } else {
            *(inst_itr++) = (struct sxcscript_inst){parsed_itr->kind, parsed_itr->val.literal};
        }
    }
    for (struct sxcscript_inst* inst_itr = sxcscript->inst; inst_itr->kind != sxcscript_kind_null; inst_itr++) {
        if (inst_itr->kind == sxcscript_kind_jmp || inst_itr->kind == sxcscript_kind_jze) {
            inst_itr->val = sxcscript->label[inst_itr->val].inst_i;
        }
    }
}
void sxcscript_init(struct sxcscript* sxcscript, const char* src) {
    sxcscript_node_init(sxcscript);
    sxcscript_tokenize(src, sxcscript->token);
    sxcscript_parse(sxcscript);
    sxcscript_analyze(sxcscript);
    sxcscript_toinst(sxcscript);
}
void sxcscript_exec(struct sxcscript* sxcscript) {
    int32_t* ip = &(sxcscript->mem[sxcscript_global_ip]);
    int32_t* sp = &(sxcscript->mem[sxcscript_global_sp]);
    int32_t* bp = &(sxcscript->mem[sxcscript_global_bp]);
    *ip = 0;
    *sp = 256;
    *bp = 128;
    while (sxcscript->inst[*ip].kind != sxcscript_kind_null) {
        switch (sxcscript->inst[*ip].kind) {
            case sxcscript_kind_const_get:
                sxcscript->mem[(*sp)++] = sxcscript->inst[*ip].val;
                break;
            case sxcscript_kind_local_get:
                sxcscript->mem[(*sp)++] = sxcscript->mem[*bp + sxcscript->mem[*sp - 1]];
                break;
            case sxcscript_kind_local_set:
                sxcscript->mem[*bp + sxcscript->mem[*sp - 2]] = sxcscript->mem[*sp - 1];
                *sp -= 2;
                break;
            case sxcscript_kind_add:
                sxcscript->mem[*sp - 2] += sxcscript->mem[*sp - 1];
                *sp -= 1;
                break;
            case sxcscript_kind_sub:
                sxcscript->mem[*sp - 2] -= sxcscript->mem[*sp - 1];
                *sp -= 1;
                break;
            case sxcscript_kind_mul:
                sxcscript->mem[*sp - 2] *= sxcscript->mem[*sp - 1];
                *sp -= 1;
                break;
            case sxcscript_kind_div:
                sxcscript->mem[*sp - 2] /= sxcscript->mem[*sp - 1];
                *sp -= 1;
                break;
            case sxcscript_kind_mod:
                sxcscript->mem[*sp - 2] %= sxcscript->mem[*sp - 1];
                *sp -= 1;
                break;
            case sxcscript_kind_jmp:
                *ip = sxcscript->inst[*ip].val - 1;
                break;
            case sxcscript_kind_jze:
                if (sxcscript->mem[*sp - 1] == 0) {
                    *ip = sxcscript->inst[*ip].val - 1;
                }
                *sp -= 1;
                break;
        }
        (*ip)++;
    }
}
int main() {
    char src[sxcscript_capacity];
    static struct sxcscript sxcscript;

    int fd = open(sxcscript_path, O_RDONLY);
    int src_n = read(fd, src, sizeof(src) - 1);
    src[src_n] = '\0';
    close(fd);
    write(STDOUT_FILENO, src, src_n);
    write(STDOUT_FILENO, "\n", 1);

    sxcscript_init(&sxcscript, src);

    sxcscript_exec(&sxcscript);

    return 0;
}