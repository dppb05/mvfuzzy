#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "medoids.h"
#include "lweight.h"
#include "model.h"
#include "stex.h"

void model_init(int objc, int clustc, int dmatrixc, int medoids_card,
        int *labels, int classc, double sample_perc) {
    init_st_matrix(&weights, clustc, dmatrixc);
    init_st_matrix(&sav_weights, clustc, dmatrixc);
    init_st_matrix(&memb, objc, clustc);
    init_st_matrix(&sav_memb, objc, clustc);
    medoids = malloc(sizeof(size_t *) * clustc);
    sav_medoids = malloc(sizeof(size_t *) * clustc);
    size_t k;
    for(k = 0; k < clustc; ++k) {
        medoids[k] = malloc(sizeof(size_t) * medoids_card);
        sav_medoids[k] = malloc(sizeof(size_t) * medoids_card);
    }
    medoids_ncol = medoids_card;

    // generating constraints
    st_matrix *groups = asgroups(labels, objc, classc);
    if(debug) {
        print_groups(groups);
    }
    int **sample = gen_sample(groups, sample_perc);
    print_header(" Sample ", HEADER_SIZE);
    print_groups_(sample, classc);
    free_st_matrix(groups);
    free(groups);
    constr = gen_constraints(sample, classc, objc);
    print_constraints(constr, objc);
    for(k = 0; k < classc; ++k) {
        free(sample[k]);
    }
    free(sample);
}

void update_memb(st_matrix *dmatrix, double mfuzval) {
    size_t e;
    size_t h;
    size_t i;
    size_t j;
    size_t k;
    double vals[memb.ncol];
    double dsum;
    double new_memb;
    int zeroc;
    for(i = 0; i < memb.nrow; ++i) {
        zeroc = 0;
        for(k = 0; k < memb.ncol; ++k) {
            vals[k] = 0.0;
            for(j = 0; j < weights.ncol; ++j) {
                dsum = 0.0;
                for(e = 0; e < medoids_ncol; ++e) {
                    dsum += get(&dmatrix[j], i, medoids[k][e]);
                }
                vals[k] += get(&weights, k, j) * dsum;
            }
            if(deq(vals[k], 0.0)) {
                ++zeroc;
            }
        }
        if(zeroc) {
            new_memb = 1.0 / (double) zeroc;
            for(k = 0; k < memb.ncol; ++k) {
                if(deq(vals[k], 0.0)) {
                    set(&memb, i, k, new_memb);
                } else {
                    set(&memb, i, k, 0.0);
                }
            }
        } else {
            for(k = 0; k < memb.ncol; ++k) {
                new_memb = 0.0;
                for(h = 0; h < memb.ncol; ++h) {
                    new_memb += pow(vals[k] / vals[h], mfuzval);
                }
                set(&memb, i, k, 1.0 / new_memb);
            }
        }
    }
}

void update_memb_constr(st_matrix *dmatrix, double alpha) {
    size_t c;
    size_t e;
    size_t i;
    size_t j;
    size_t k;
    double mtx_a[memb.nrow][memb.ncol];
    bool set_a[memb.nrow][memb.ncol];
    size_t set_a_c[memb.nrow];
    double val;
    double dsum;
    for(i = 0; i < memb.nrow; ++i) {
        set_a_c[i] = 0;
        for(k = 0; k < memb.ncol; ++k) {
            val = 0.0;
            for(j = 0; j < weights.ncol; ++j) {
                dsum = 0.0;
                for(e = 0; e < medoids_ncol; ++e) {
                    dsum += get(&dmatrix[j], i, medoids[k][e]);
                }
                val += get(&weights, k, j) * dsum;
            }
            mtx_a[i][k] = val;
            if(deq(val, 0.0)) {
                set_a[i][k] = true;
                set_a_c[i] += 1;
            } else {
                set_a[i][k] = false;
            }
        }
    }
    bool set_v[memb.nrow][memb.ncol];
    double mtx_b[memb.nrow][memb.ncol];
    int obj;
    for(i = 0; i < memb.nrow; ++i) {
        if(!set_a_c[i]) {
            for(k = 0; k < memb.ncol; ++k) {
                set_v[i][k] = true;
                mtx_a[i][k] *= 2.0;
                val = 0.0;
                if(constr[i]) {
                    for(e = 0; e < constr[i]->ml->size; ++e) {
                        obj = constr[i]->ml->get[e];
                        for(c = 0; c < memb.ncol; ++c) {
                            if(c != k) {
                                val += get(&memb, obj, c);
                            }
                        }
                    }
                    for(e = 0; e < constr[i]->mnl->size; ++e) {
                        obj = constr[i]->mnl->get[e];
                        val += get(&memb, obj, k);
                    }
//                    val *= 2.0;
                }
                mtx_b[i][k] = alpha * val;
            }
        }
    }
    double sum_num;
    double sum_den;
    double gamma;
    bool test;
    do {
        test = false;
        for(i = 0; i < memb.nrow; ++i) {
            if(!set_a_c[i]) {
                sum_num = 0.0;
                sum_den = 0.0;
                for(k = 0; k < memb.ncol; ++k) {
                    if(set_v[i][k]) {
                        sum_num += mtx_b[i][k] / mtx_a[i][k];
                        sum_den += 1.0 / mtx_a[i][k];
                    }
                }
                gamma = (1.0 + sum_num) / sum_den;
                for(k = 0; k < memb.ncol; ++k) {
                    if(set_v[i][k]) {
                        val = (gamma - mtx_b[i][k]) / mtx_a[i][k];
                        if(dgt(val, 0.0)) {
                            set(&memb, i, k, val);
                        } else {
                            set(&memb, i, k, 0.0);
                            set_v[i][k] = false;
                            test = true;
                        }
                    }
                }
            }
        }
    } while(test);
    for(i = 0; i < memb.nrow; ++i) {
        if(set_a_c[i]) {
            val = 1.0 / (double) set_a_c[i];
            for(k = 0; k < memb.ncol; ++k) {
                if(set_a[i][k]) {
                    set(&memb, i, k, val);
                } else {
                    set(&memb, i, k, 0.0);
                }
            }
        }
    }
}

void print_memb(st_matrix *memb) {
	print_header("Membership", HEADER_SIZE);
	size_t i;
	size_t k;
	double sum;
	for(i = 0; i < memb->nrow; ++i) {
		printf("%u: ", i);
		sum = 0.0;
		for(k = 0; k < memb->ncol; ++k) {
			printf("%lf ", get(memb, i, k));
            sum += get(memb, i, k);
		}
		printf("[%lf]", sum);
		if(!deq(sum, 1.0)) {
			printf("*\n");
		} else {
			printf("\n");
		}
	}
}

double constr_adequacy(st_matrix *dmatrix, double alpha) {
    size_t c;
    size_t e;
    size_t i;
    size_t k;
    int obj;
    double adeq = 0.0;
    for(i = 0; i < memb.nrow; ++i) {
        if(constr[i]) {
            for(e = 0; e < constr[i]->ml->size; ++e) {
                obj = constr[i]->ml->get[e];
                for(c = 0; c < memb.ncol; ++c) {
                    for(k = 0; k < memb.ncol; ++k) {
                        if(c != k) {
                            adeq += get(&memb, i, c) *
                                        get(&memb, obj, k);
                        }
                    }
                }
            }
            for(e = 0; e < constr[i]->mnl->size; ++e) {
                obj = constr[i]->mnl->get[e];
                for(c = 0; c < memb.ncol; ++c) {
                    adeq += get(&memb, i, c) * get(&memb, obj, c);
                }
            }
        }
    }
    return alpha * adeq;
}

double prev_adeq_unconstr;
double prev_adeq_constr;

double adequacy(st_matrix *dmatrix, double mfuz, double alpha) {
    size_t e;
    size_t i;
    size_t j;
    size_t k;
    double adeq = 0.0;
    double dsum;
    double wsum;
    for(k = 0; k < memb.ncol; ++k) {
        for(i = 0; i < memb.nrow; ++i) {
            wsum = 0.0;
            for(j = 0; j < weights.ncol; ++j) {
                dsum = 0.0;
                for(e = 0; e < medoids_ncol; ++e) {
                    dsum += get(&dmatrix[j], i, medoids[k][e]);
                }
                wsum += get(&weights, k, j) * dsum;
            }
            adeq += pow(get(&memb, i, k), mfuz) * wsum;
        }
    }
    double cadeq = constr_adequacy(dmatrix, alpha);
    if(debug) {
        printf("[Debug]Adequacy: %.15lf %.15lf\n", adeq, cadeq);
        if(prev_adeq_unconstr && dgt(adeq, prev_adeq_unconstr)) {
            printf("[Debug] current unconstrained adequacy is greater"
                    " than previous.\n");
        }
        if(prev_adeq_constr && dgt(cadeq, prev_adeq_constr)) {
            printf("[Debug] current constrained adequacy is greater "
                    "than previous.\n");
        }
        prev_adeq_unconstr = adeq;
        prev_adeq_constr = cadeq;
    }
    return adeq + cadeq;
}

void save_env() {
    mtxcpy(&sav_weights, &weights);
    mtxcpy(&sav_memb, &memb);
    mtxcpy_size_t(sav_medoids, medoids, memb.ncol, medoids_ncol);
}

void print_env() {
    print_weights(&weights);
    print_medoids(medoids, memb.ncol, medoids_ncol);
    print_memb(&memb);
}

double run(st_matrix *dmatrix, int max_iter, double epsilon,
        double theta, double mfuz, double alpha) {
    double mfuzval = 1.0 / (mfuz - 1.0);
    int objc = memb.nrow;
    int clustc = memb.ncol;
    int medoids_card = medoids_ncol;
    prev_adeq_unconstr = 0.0;
    prev_adeq_constr = 0.0;

    init_weights(&weights);
    print_weights(&weights);
    init_medoids(medoids, objc, clustc, medoids_card);
    print_medoids(medoids, clustc, medoids_card);
    update_memb(dmatrix, mfuzval);
    print_memb(&memb);
    double prev_adeq = adequacy(dmatrix, mfuz, alpha);
    printf("\nAdequacy: %.15lf\n", prev_adeq);
    double cur_adeq;
    double prev_step_adeq = prev_adeq; // for debug
    double cur_step_adeq; // for debug
    double adeq_diff;
    size_t it;
    for(it = 0; it < max_iter; ++it) {
        printf("\nIteration %d\n", it);
        update_medoids_lw(medoids, medoids_card, &memb, &weights,
                dmatrix, mfuz);
        if(verbose) {
            print_medoids(medoids, clustc, medoids_card);
        }
        if(debug) {
            cur_step_adeq = adequacy(dmatrix, mfuz, alpha);
            // no need to check for first iter in this model
            if(it) {
                adeq_diff = prev_step_adeq - cur_step_adeq;
                if(dlt(adeq_diff, 0.0)) {
                    printf("[Warn] current step adequacy is greater than "
                            "previous (%.15lf)\n", - adeq_diff);
                }
            }
            prev_step_adeq = cur_step_adeq;
        }
        update_weights_md(&weights, &memb, medoids, medoids_card,
                dmatrix, theta, mfuz);
        if(verbose) {
            print_weights(&weights);
        }
        if(debug) {
            cur_step_adeq = adequacy(dmatrix, mfuz, alpha);
            adeq_diff = prev_step_adeq - cur_step_adeq;
            if(dlt(adeq_diff, 0.0)) {
                printf("[Warn] current step adequacy is greater than "
                        "previous (%.15lf)\n", - adeq_diff);
            }
            prev_step_adeq = cur_step_adeq;
        }
        update_memb_constr(dmatrix, alpha);
        if(verbose) {
            print_memb(&memb);
        }
        cur_adeq = adequacy(dmatrix, mfuz, alpha);
        if(debug) {
            cur_step_adeq = cur_adeq;
            adeq_diff = prev_step_adeq - cur_step_adeq;
            if(dlt(adeq_diff, 0.0)) {
                printf("[Warn] current step adequacy is greater than "
                        "previous (%.15lf)\n", - adeq_diff);
            }
            prev_step_adeq = cur_step_adeq;
        }
        adeq_diff = prev_adeq - cur_adeq;
        printf("\nAdequacy: %.15lf (%.15lf)\n", cur_adeq, adeq_diff);
        // no need to check for first iter in this model
        if(debug && it) {
            if(dlt(adeq_diff, 0.0)) {
                printf("[Warn] current iteration adequacy is greater"
                        " than previous (%.15lf)\n", - adeq_diff);
            }
        }
        printf("adeq_diff: %.15lf\n", adeq_diff);
        printf("epsilon: %.15lf\n", epsilon);
        if(adeq_diff < epsilon) {
            printf("Adequacy coefficient difference reached in %d "
                    "iterations.\n", it);
            break;
        }
        prev_adeq = cur_adeq;
    }
    printf("\nClustering process finished.\n");
    print_weights(&weights);
    print_medoids(medoids, clustc, medoids_card);
    print_memb(&memb);
    return cur_adeq;
}

// computes a matrix containing the distance of each object to each
// cluster prototype.
st_matrix* medoid_dist(st_matrix *dmatrix) {
    st_matrix *ret = malloc(sizeof(st_matrix));
    init_st_matrix(ret, memb.nrow, memb.ncol);
    size_t e;
    size_t i;
    size_t j;
    size_t k;
    double val;
    double sumd;
    for(i = 0; i < memb.nrow; ++i) {
        for(k = 0; k < memb.ncol; ++k) {
            val = 0.0;
            for(j = 0; j < weights.ncol; ++j) {
                sumd = 0.0;
                for(e = 0; e < medoids_ncol; ++e) {
                    sumd += get(&dmatrix[j], i, medoids[k][e]);
                }
                val += get(&weights, k, j) * sumd;
            }
            set(ret, i, k, val);
        }
    }
    return ret;
}

// aggregates all distance matrices into a single one by summing
// them up taking into account each cluster weight.
st_matrix* agg_dmatrix(st_matrix *dmatrix) {
    st_matrix *ret = malloc(sizeof(st_matrix));
    init_st_matrix(ret, memb.nrow, memb.nrow);
    size_t e;
    size_t i;
    size_t j;
    size_t k;
    double val;
    for(i = 0; i < memb.nrow; ++i) {
        for(e = 0; e < memb.nrow; ++e) {
            val = 0.0;
            for(k = 0; k < memb.ncol; ++k) {
                for(j = 0; j < weights.ncol; ++j) {
                    val += get(&weights, k, j) *
                            get(&dmatrix[j], i, e);
                }
            }
            set(ret, i, e, val);
        }
    }
    return ret;
}

void compute_idxs(st_matrix *dmatrix, int *labels, double mfuz) {
    print_header(" Indexes ", HEADER_SIZE);
    int *pred = defuz(&memb);
    printf("CR: %.10lf\n", corand(labels, pred, memb.nrow));
    st_matrix *confmtx = confusion(labels, pred, memb.nrow);
    printf("F-measure: %.10lf\n", fmeasure(confmtx, false));
    printf("Partition coefficient: %.10lf\n", partcoef(&memb));
    printf("Modified partition coefficient: %.10lf\n",
            modpcoef(&memb));
    printf("Partition entropy: %.10lf (max: %.10lf)\n",
            partent(&memb), log(memb.ncol));
    st_matrix *md_dist = medoid_dist(dmatrix);
    printf("Average intra cluster distance: %.10lf\n",
            avg_intra_dist(&memb, md_dist, mfuz));
    print_header(" Confusion matrix (class x cluster) ",
            HEADER_SIZE);
    print_st_matrix(confmtx, 0, true);

    print_header(" Predicted groups for objects ", HEADER_SIZE);
    st_matrix *groups = asgroups(pred, memb.nrow, memb.ncol);
    print_groups(groups);
    st_matrix *agg_dmtx = agg_dmatrix(dmatrix);
    silhouet *csil = crispsil(groups, agg_dmtx);
    print_header("Crisp silhouette", HEADER_SIZE);
    print_silhouet(csil);
    silhouet *fsil = fuzzysil(csil, groups, &memb, mfuz);
    print_header("Fuzzy silhouette", HEADER_SIZE);
    print_silhouet(fsil);
    silhouet *ssil = simplesil(pred, md_dist);
    print_header("Simple silhouette", HEADER_SIZE);
    print_silhouet(ssil);
    free(pred);
    free_st_matrix(confmtx);
    free(confmtx);
    free_st_matrix(groups);
    free(groups);
    free_st_matrix(agg_dmtx);
    free(agg_dmtx);
    free_st_matrix(md_dist);
    free(md_dist);
    free_silhouet(csil);
    free(csil);
    free_silhouet(fsil);
    free(fsil);
    free_silhouet(ssil);
    free(ssil);
}

void model_free() {
    if(medoids) {
        size_t k;
        for(k = 0; k < memb.ncol; ++k) {
            if(medoids[k]) {
                free(medoids[k]);
            }
        }
        free(medoids);
    }
    if(sav_medoids) {
        size_t k;
        for(k = 0; k < memb.ncol; ++k) {
            if(sav_medoids[k]) {
                free(sav_medoids[k]);
            }
        }
        free(sav_medoids);
    }
    if(constr) {
        size_t i;
        for(i = 0; i < memb.nrow; ++i) {
            if(constr[i]) {
                constraint_free(constr[i]);
            }
        }
        free(constr);
    }
    free_st_matrix(&weights);
    free_st_matrix(&sav_weights);
    free_st_matrix(&memb);
    free_st_matrix(&sav_memb);
}

