//
//  vertexrelabel_static.cpp
//  graphchi_xcode
//
//  Created by Michael Hahn on 2/15/17.
//
//

//Use struct as type values in edgelist format
//Income data format should be in the form: src_id dst_id src_type:dst_type:edge_type
//Unlike dynamic edge data, the struct size does not change
//We use an auxiliary in-memory data structure to update the type value after the first iteration
//types must be integers. For example, mmap_write should be 12.

#include <string>
#include <iostream>
#include <stdlib.h>
#include <map>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <cassert>
#include <cmath>
#include <algorithm>
#include "graphchi_basic_includes.hpp"
#include "logger/logger.hpp"

using namespace graphchi;

#define METRIC 0

/**
 * Type definitions. Remember to create suitable graph shards using the
 * Sharder-program.
 */
struct type_label {
    int old_src;
    int old_dst;
    int new_src;
    int new_dst;
    int edge;
};


typedef int VertexDataType;
typedef type_label EdgeDataType;//src_type dst_type edge_type

class profile {
public:
    double get_mean() {
        return this->mean;
    }
    
    double get_std() {
        return this->std;
    }
    
    void set_mean(double mean) {
        this->mean = mean;
    }
    
    void set_std(double std) {
        this->std = std;
    }
    
    double mean;
    double std;
    std::vector<std::vector<int>> count_arrays;
};

class KernelMaps {
public:
    KernelMaps(int counter);
    ~KernelMaps();
    void resetMaps(int counter);
    
    int insert_relabel(std::string label);
    
    //insert int to label_map if it does not exist, or update the mapped value otherwise.
    void insert_label(std::map<int, int>& lmap, int label);
    
    void print_relabel_map();
    
    void print_label_map(std::map<int, int>lmap);
    
    int calculate_kernel(std::map<int, int>& map1, std::map<int, int>& map2);
    
    std::map<int, int> label_map;//for the first graph
    std::map<int, int> label_map_2;//for the second graph
    std::map<std::string, int> relabel_map;//for both graphs
    int counter;
};

KernelMaps::KernelMaps(int counter) {
    counter = this->counter;
}

KernelMaps::~KernelMaps() {
    label_map.clear();
    relabel_map.clear();
    label_map_2.clear();
}

void KernelMaps::resetMaps(int counter) {
    counter = this->counter;
    label_map.clear();
    //relabel_map.clear();//Bug here: this map should not be reset at all.
    label_map_2.clear();
}

int KernelMaps::insert_relabel(std::string label) {
    std::pair<std::map<std::string, int>::iterator, bool> rst;
    rst = relabel_map.insert(std::pair<std::string, int>(label, counter));
    if (rst.second == false) {
        logstream(LOG_INFO) << "Label " + label + " is already in the map." << std::endl;
        return rst.first->second;
    } else {
        counter++;
        return counter - 1;
    }
}

//insert int to label_map if it does not exist, or update the mapped value otherwise.
void KernelMaps::insert_label(std::map<int, int>& lmap, int label) {
    std::pair<std::map<int, int>::iterator, bool> rst;
    rst = lmap.insert(std::pair<int, int>(label, 1));
    if (rst.second == false) {
        logstream(LOG_INFO) << "Label is already in the map. Updating the value..." << std::endl;
        rst.first->second++;
    }
    return;
}

void KernelMaps::print_relabel_map () {
    std::map<std::string, int>::iterator map_itr;
    logstream(LOG_INFO) << "Printing relabel map..." << std::endl;
    for (map_itr = relabel_map.begin(); map_itr != relabel_map.end(); map_itr++)
        logstream(LOG_INFO) << map_itr->first << ":" << map_itr->second << std::endl;
}

void KernelMaps::print_label_map (std::map<int, int>lmap) {
    std::map<int, int>::iterator map_itr;
    logstream(LOG_INFO) << "Printing label map..." << std::endl;
    for (map_itr = lmap.begin(); map_itr != lmap.end(); map_itr++)
        logstream(LOG_INFO) << map_itr->first << ":" << map_itr->second << std::endl;
}

int KernelMaps::calculate_kernel(std::map<int, int>& map1, std::map<int, int>& map2) {
    int sum = 0;
    int arr_size = 0;
    int map1_size = map1.rbegin()->first;
    int map2_size = map2.rbegin()->first;
    if (map1_size >= map2_size)
        arr_size = map1_size;
    else
        arr_size = map2_size;
    std::vector<int> map1_arr;
    std::vector<int> map2_arr;
    std::map<int, int>::iterator map_itr_1 = map1.begin();
    std::map<int, int>::iterator map_itr_2 = map2.begin();
    for (int i = 0; i < arr_size; i++) {
        if (map_itr_1 != map1.end()) {
            if (map_itr_1->first == i) {
                map1_arr.push_back(map_itr_1->second);
                map_itr_1++;
            } else map1_arr.push_back(0);
        } else {
            map1_arr.push_back(0);
        }
        if (map_itr_2 != map2.end()) {
            if (map_itr_2->first == i) {
                map2_arr.push_back(map_itr_2->second);
                map_itr_2++;
            } else map2_arr.push_back(0);
        } else {
            map2_arr.push_back(0);
        }
        //Sum of multiplication
        if (METRIC == 0) {
            sum += map1_arr.back() * map2_arr.back();
        }
        //Sum of geometric distance SQUARED
        else if (METRIC == 1) {
            sum += (map1_arr.back() - map2_arr.back()) * (map1_arr.back() * map2_arr.back());
        }
    }
    //For debug only:
    //*******************
    std::cout << "Array1: ";
    for (std::vector<int>::iterator it = map1_arr.begin(); it != map1_arr.end(); it++) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    std::cout << "Array2: ";
    for (std::vector<int>::iterator it = map2_arr.begin(); it != map2_arr.end(); it++) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    //********************
    return sum;
}


// Parse the type value in the file to the type_label structure for reading
void parse(type_label &x, const char * s) {
    char * ss = (char *) s;
    char delims[] = ":";
    char * t;
    t = strtok(ss, delims);
    if (t == NULL)
        logstream(LOG_FATAL) << "Source Type info does not exist" << std::endl;
    assert(t != NULL);
    x.new_src = atoi(t);
    //TODO: We can make sure type value is never 0 so we can check if parse goes wrong here
    t = strtok(NULL, delims);
    if (t == NULL)
        logstream(LOG_FATAL) << "Destination Type info does not exist" << std::endl;
    assert (t != NULL);
    x.new_dst = atoi(t);
    t = strtok(NULL, delims);
    if (t == NULL)
        logstream(LOG_FATAL) << "Edge Type info does not exist" << std::endl;
    assert (t != NULL);
    x.edge = atoi(t);
    t = strtok(NULL, delims);
    if (t != NULL)
        logstream(LOG_FATAL) << "Extra info will be ignored" << std::endl;
    return;
}

//global kernelmaps instance
KernelMaps km(0);

//global vector that holds the kernel value for each pair of graphs
//the position of the element in this vector indicates the identity of the value.
//for three graphs numbered 1, 2, 3, the vector contains: [1-1, 1-2, 1-3, 2-2, 2-3, 3-3].
std::vector<int> kv;

//global version macro
//0: each vertex takes both incoming and outgoing neighboring vertices' labels, sorts them, and combines with its own label to relabel. No direction or edge labels considered
//1: each vertex takes its incoming neighboring vertices' labels, sorts them, and combines with its own label to relabel. Then it takes its outgoing neighboring vertices' labels, sorts them, and combines with its own label to relabel. Then it uses these two labels, sorts them and then relabels. No edge labels considered
#define VERSION 2
#define TAKEEDGELABEL 0

/**
 * GraphChi programs need to subclass GraphChiProgram<vertex-type, edge-type>
 * class. The main logic is usually in the update function.
 */
struct VertexRelabel : public GraphChiProgram<VertexDataType, EdgeDataType> {
 
    //set two maps, and a counter to be in KernelMaps instance
//    std::map<int, int> label_map;
//    std::map<std::string, int> relabel_map;
//    int counter = 0;
    std::mutex relabel_map_lock;
    std::mutex label_map_lock;
    /**
     *  Vertex update function.
     */
    void update(graphchi_vertex<VertexDataType, EdgeDataType> &vertex, graphchi_context &gcontext) {
        //TODO: can scheduling make a difference so that label updates can be synchronized?
        //assert(gcontext.scheduler != NULL);
        if (gcontext.iteration % 2 == 1) {
            for(int i=0; i < vertex.num_inedges(); i++) {
                graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                type_label in_type = in_edge->get_data();
                in_type.old_dst = in_type.new_dst;
                in_edge->set_data(in_type);
                logstream(LOG_INFO) << "Swapped in edges of " << vertex.id() << " to " << in_type.old_dst << std::endl;
            }
            for (int i=0; i < vertex.num_outedges(); i++) {
                graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                type_label out_type = out_edge->get_data();
                out_type.old_src = out_type.new_src;
                out_edge->set_data(out_type);
                logstream(LOG_INFO) << "Swapped out edges of " << vertex.id() << " to " << out_type.old_src << std::endl;
            }
        } else {
            if (gcontext.iteration == 0) {
                /* On first iteration, initialize vertex (and its edges). This is usually required, because
                 on each run, GraphChi will modify the data files. To start from scratch, it is easiest
                 do initialize the program in code. Alternatively, you can keep a copy of initial data files. */
                // First for each vertex, set its label as its w3ctype
                std::string vertex_label = "";
                // The value can be obtained from any outedge (from src_type) or in_edge from (dst_type)
                graphchi_edge<EdgeDataType> * outedge = vertex.random_outedge();
                //if the node has no outedge, we get a first inedge in the queue
                if (outedge == NULL) {
                    graphchi_edge<EdgeDataType> * inedge = vertex.inedge(0);
                    //get the dst_type from inedge
                    int vertex_int_label = inedge->get_data().new_dst;
                    std::string num_string;
                    std::stringstream out;
                    out << vertex_int_label;
                    vertex_label = out.str();
                } else {
                    //get the src_type from outedge
                    int vertex_int_label = outedge->get_data().new_src;
                    std::string num_string;
                    std::stringstream out;
                    out << vertex_int_label;
                    vertex_label = out.str();
                }
                //make sure vertex must have a valid string, not an empty string
                if (vertex_label != "") {
                    relabel_map_lock.lock();
                    int label_map_label = km.insert_relabel(vertex_label);
                    relabel_map_lock.unlock();
                    label_map_lock.lock();
                    km.insert_label(km.label_map, label_map_label);
                    label_map_lock.unlock();
                    vertex.set_data(label_map_label);
                    logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label << std::endl;
                } else {
                    logstream(LOG_FATAL) << "Invalid vertex_label in relabel_map. " << std::endl;
                    assert (vertex_label != "");
                }
            } else {
                if (VERSION == 0) {
                    std::vector<int> label_vec;
                    for(int i=0; i < vertex.num_inedges(); i++) {
                        graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                        int int_in_type = in_edge->get_data().old_src;
                        label_vec.push_back(int_in_type);
                        logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_type << " from in edges" << std::endl;
                        if (TAKEEDGELABEL == 1 && gcontext.iteration == 2) {
                            int int_in_edge_type = in_edge->get_data().edge;
                            label_vec.push_back(int_in_edge_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_edge_type << " (edge type) from in edges" << std::endl;
                        }
                    }
                    for (int i=0; i < vertex.num_outedges(); i++) {
                        graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                        int int_out_type = out_edge->get_data().old_dst;
                        label_vec.push_back(int_out_type);
                        logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_type << " from out edges" << std::endl;
                        if (TAKEEDGELABEL == 1 && gcontext.iteration == 2) {
                            int int_out_edge_type = out_edge->get_data().edge;
                            label_vec.push_back(int_out_edge_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_edge_type << " (edge type) from in edges" << std::endl;
                        }
                    }
                    std::sort(label_vec.begin(), label_vec.end());
                    int self_label = vertex.get_data();
                    label_vec.push_back(self_label);
                    
                    std::string new_label = "";
                    std::string first_num_string;
                    std::stringstream first_out;
                    first_out << *(label_vec.end() - 1);
                    first_num_string = first_out.str();
                    new_label += first_num_string;
                    new_label += ",";

                    for (std::vector<int>::iterator it = label_vec.begin(); it != label_vec.end() - 1; ++it) {
                        std::string num_string;
                        std::stringstream out;
                        out << *it;
                        num_string = out.str();
                        new_label += num_string + " ";
                    }
                    
                    relabel_map_lock.lock();
                    int label_map_label = km.insert_relabel(new_label);
                    relabel_map_lock.unlock();
                    label_map_lock.lock();
                    km.insert_label(km.label_map, label_map_label);
                    label_map_lock.unlock();
                    vertex.set_data(label_map_label);
                    logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label << std::endl;
                }
                if (VERSION == 1) {
                    std::vector<int> incoming_label_vec;
                    std::vector<int> outgoing_label_vec;
                    for(int i=0; i < vertex.num_inedges(); i++) {
                        graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                        int int_in_type = in_edge->get_data().old_src;
                        incoming_label_vec.push_back(int_in_type);
                        logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_type << " from in edges" << std::endl;
                        if (TAKEEDGELABEL == 1 && gcontext.iteration == 2) {
                            int int_in_edge_type = in_edge->get_data().edge;
                            incoming_label_vec.push_back(int_in_edge_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_edge_type << " (edge type) from in edges" << std::endl;
                        }
                    }
                    for (int i=0; i < vertex.num_outedges(); i++) {
                        graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                        int int_out_type = out_edge->get_data().old_dst;
                        outgoing_label_vec.push_back(int_out_type);
                        logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_type << " from out edges" << std::endl;
                        if (TAKEEDGELABEL == 1 && gcontext.iteration == 2) {
                            int int_out_edge_type = out_edge->get_data().edge;
                            outgoing_label_vec.push_back(int_out_edge_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_edge_type << " (edge type) from in edges" << std::endl;
                        }
                    }
                    std::sort(incoming_label_vec.begin(), incoming_label_vec.end());
                    std::sort(outgoing_label_vec.begin(), outgoing_label_vec.end());
                    
                    int self_label = vertex.get_data();
                    incoming_label_vec.push_back(self_label);
                    outgoing_label_vec.push_back(self_label);
                    
                    std::string new_incoming_label = "";
                    std::string first_incoming_num_string;
                    std::stringstream first_incoming_out;
                    first_incoming_out << *(incoming_label_vec.end() - 1);
                    first_incoming_num_string = first_incoming_out.str();
                    new_incoming_label += first_incoming_num_string;
                    new_incoming_label += ",";
                    
                    for (std::vector<int>::iterator it = incoming_label_vec.begin(); it != incoming_label_vec.end() - 1; ++it) {
                        std::string incoming_num_string;
                        std::stringstream incoming_out;
                        incoming_out << *it;
                        incoming_num_string = incoming_out.str();
                        new_incoming_label += incoming_num_string + " ";
                    }
                    
                    std::string new_outgoing_label = "";
                    std::string first_outgoing_num_string;
                    std::stringstream first_outgoing_out;
                    first_outgoing_out << *(outgoing_label_vec.end() - 1);
                    first_outgoing_num_string = first_outgoing_out.str();
                    new_outgoing_label += first_outgoing_num_string;
                    new_outgoing_label += ",";
                    
                    for (std::vector<int>::iterator it = outgoing_label_vec.begin(); it != outgoing_label_vec.end() - 1; ++it) {
                        std::string outgoing_num_string;
                        std::stringstream outgoing_out;
                        outgoing_out << *it;
                        outgoing_num_string = outgoing_out.str();
                        new_outgoing_label += outgoing_num_string + " ";
                    }
                    
                    relabel_map_lock.lock();
                    int label_map_label_incoming = km.insert_relabel(new_incoming_label);
                    int label_map_label_outgoing = km.insert_relabel(new_outgoing_label);
                    relabel_map_lock.unlock();
                    
                    std::string new_combined_label = ""; //label_map_label_incoming,label_map_label_outgoing
                    std::string incoming_label_in_combined;
                    std::stringstream incoming_label_in_combined_out;
                    incoming_label_in_combined_out << label_map_label_incoming;
                    incoming_label_in_combined = incoming_label_in_combined_out.str();
                    new_combined_label += incoming_label_in_combined;
                    new_combined_label += ",";
                    std::string outgoing_label_in_combined;
                    std::stringstream outgoing_label_in_combined_out;
                    outgoing_label_in_combined_out << label_map_label_outgoing;
                    outgoing_label_in_combined = outgoing_label_in_combined_out.str();
                    new_combined_label += outgoing_label_in_combined;
                    
                    relabel_map_lock.lock();
                    int label_map_label_combined = km.insert_relabel(new_combined_label);
                    relabel_map_lock.unlock();
                    
                    label_map_lock.lock();
                    km.insert_label(km.label_map, label_map_label_combined);
                    label_map_lock.unlock();
                    vertex.set_data(label_map_label_combined);
                    logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label_combined << std::endl;
                }
                if (VERSION == 2) {
                    if (gcontext.iteration == 2) {
                        std::vector<std::pair<int, int>> incoming_pair_label_vec;
                        std::vector<std::pair<int, int>> outgoing_pair_label_vec;
                        for(int i=0; i < vertex.num_inedges(); i++) {
                            graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                            int int_in_type = in_edge->get_data().old_src;
                            int int_in_edge_type = in_edge->get_data().edge;
                            std::pair<int, int> pair_label_in (int_in_type, int_in_edge_type);
                            incoming_pair_label_vec.push_back(pair_label_in);
                        }
                        for (int i=0; i < vertex.num_outedges(); i++) {
                            graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                            int int_out_type = out_edge->get_data().old_dst;
                            int int_out_edge_type = out_edge->get_data().edge;
                            std::pair<int, int> pair_label_out (int_out_type, int_out_edge_type);
                            outgoing_pair_label_vec.push_back(pair_label_out);
                        }
                        struct {
                            bool operator()(std::pair<int, int> a, std::pair<int, int> b) {
                                return a.first < b.first;
                            }   
                        } comparePair;
                        
                        std::sort(incoming_pair_label_vec.begin(), incoming_pair_label_vec.end(), comparePair);
                        std::sort(outgoing_pair_label_vec.begin(), outgoing_pair_label_vec.end(), comparePair);
                        
                        std::string new_incoming_label = "";
                        std::string first_incoming_num_string;
                        std::stringstream first_incoming_out;
                        first_incoming_out << vertex.get_data();
                        first_incoming_num_string = first_incoming_out.str();
                        new_incoming_label += first_incoming_num_string;
                        new_incoming_label += ",";
                        
                        for (std::vector<std::pair<int, int>>::iterator it = incoming_pair_label_vec.begin(); it != incoming_pair_label_vec.end(); ++it) {
                            std::string incoming_num_string_first;
                            std::stringstream incoming_out_first;
                            incoming_out_first << it->first;
                            incoming_num_string_first = incoming_out_first.str();
                            new_incoming_label += incoming_num_string_first + " ";
                            std::string incoming_num_string_second;
                            std::stringstream incoming_out_second;
                            incoming_out_second << it->second;
                            incoming_num_string_second = incoming_out_second.str();
                            new_incoming_label += incoming_num_string_second + " ";
                        }
                        
                        std::string new_outgoing_label = "";
                        std::string first_outgoing_num_string;
                        std::stringstream first_outgoing_out;
                        first_outgoing_out << vertex.get_data();
                        first_outgoing_num_string = first_outgoing_out.str();
                        new_outgoing_label += first_outgoing_num_string;
                        new_outgoing_label += ",";
                        
                        for (std::vector<std::pair<int, int>>::iterator it = outgoing_pair_label_vec.begin(); it != outgoing_pair_label_vec.end(); ++it) {
                            std::string outgoing_num_string_first;
                            std::stringstream outgoing_out_first;
                            outgoing_out_first << it->first;
                            outgoing_num_string_first = outgoing_out_first.str();
                            new_incoming_label += outgoing_num_string_first + " ";
                            std::string outgoing_num_string_second;
                            std::stringstream outgoing_out_second;
                            outgoing_out_second << it->second;
                            outgoing_num_string_second = outgoing_out_second.str();
                            new_incoming_label += outgoing_num_string_second + " ";
                        }
                        
                        relabel_map_lock.lock();
                        int label_map_label_incoming = km.insert_relabel(new_incoming_label);
                        int label_map_label_outgoing = km.insert_relabel(new_outgoing_label);
                        relabel_map_lock.unlock();
                        
                        std::string new_combined_label = ""; //label_map_label_incoming,label_map_label_outgoing
                        std::string incoming_label_in_combined;
                        std::stringstream incoming_label_in_combined_out;
                        incoming_label_in_combined_out << label_map_label_incoming;
                        incoming_label_in_combined = incoming_label_in_combined_out.str();
                        new_combined_label += incoming_label_in_combined;
                        new_combined_label += ",";
                        std::string outgoing_label_in_combined;
                        std::stringstream outgoing_label_in_combined_out;
                        outgoing_label_in_combined_out << label_map_label_outgoing;
                        outgoing_label_in_combined = outgoing_label_in_combined_out.str();
                        new_combined_label += outgoing_label_in_combined;
                        
                        relabel_map_lock.lock();
                        int label_map_label_combined = km.insert_relabel(new_combined_label);
                        relabel_map_lock.unlock();
                        
                        label_map_lock.lock();
                        km.insert_label(km.label_map, label_map_label_combined);
                        label_map_lock.unlock();
                        vertex.set_data(label_map_label_combined);
                        logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label_combined << std::endl;
                    } else {
                        std::vector<int> incoming_label_vec;
                        std::vector<int> outgoing_label_vec;
                        for(int i=0; i < vertex.num_inedges(); i++) {
                            graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                            int int_in_type = in_edge->get_data().old_src;
                            incoming_label_vec.push_back(int_in_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_type << " from in edges" << std::endl;
                        }
                        for (int i=0; i < vertex.num_outedges(); i++) {
                            graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                            int int_out_type = out_edge->get_data().old_dst;
                            outgoing_label_vec.push_back(int_out_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_type << " from out edges" << std::endl;
                        }
                        std::sort(incoming_label_vec.begin(), incoming_label_vec.end());
                        std::sort(outgoing_label_vec.begin(), outgoing_label_vec.end());
                        
                        int self_label = vertex.get_data();
                        incoming_label_vec.push_back(self_label);
                        outgoing_label_vec.push_back(self_label);
                        
                        std::string new_incoming_label = "";
                        std::string first_incoming_num_string;
                        std::stringstream first_incoming_out;
                        first_incoming_out << *(incoming_label_vec.end() - 1);
                        first_incoming_num_string = first_incoming_out.str();
                        new_incoming_label += first_incoming_num_string;
                        new_incoming_label += ",";
                        
                        for (std::vector<int>::iterator it = incoming_label_vec.begin(); it != incoming_label_vec.end() - 1; ++it) {
                            std::string incoming_num_string;
                            std::stringstream incoming_out;
                            incoming_out << *it;
                            incoming_num_string = incoming_out.str();
                            new_incoming_label += incoming_num_string + " ";
                        }
                        
                        std::string new_outgoing_label = "";
                        std::string first_outgoing_num_string;
                        std::stringstream first_outgoing_out;
                        first_outgoing_out << *(outgoing_label_vec.end() - 1);
                        first_outgoing_num_string = first_outgoing_out.str();
                        new_outgoing_label += first_outgoing_num_string;
                        new_outgoing_label += ",";
                        
                        for (std::vector<int>::iterator it = outgoing_label_vec.begin(); it != outgoing_label_vec.end() - 1; ++it) {
                            std::string outgoing_num_string;
                            std::stringstream outgoing_out;
                            outgoing_out << *it;
                            outgoing_num_string = outgoing_out.str();
                            new_outgoing_label += outgoing_num_string + " ";
                        }
                        
                        relabel_map_lock.lock();
                        int label_map_label_incoming = km.insert_relabel(new_incoming_label);
                        int label_map_label_outgoing = km.insert_relabel(new_outgoing_label);
                        relabel_map_lock.unlock();
                        
                        std::string new_combined_label = ""; //label_map_label_incoming,label_map_label_outgoing
                        std::string incoming_label_in_combined;
                        std::stringstream incoming_label_in_combined_out;
                        incoming_label_in_combined_out << label_map_label_incoming;
                        incoming_label_in_combined = incoming_label_in_combined_out.str();
                        new_combined_label += incoming_label_in_combined;
                        new_combined_label += ",";
                        std::string outgoing_label_in_combined;
                        std::stringstream outgoing_label_in_combined_out;
                        outgoing_label_in_combined_out << label_map_label_outgoing;
                        outgoing_label_in_combined = outgoing_label_in_combined_out.str();
                        new_combined_label += outgoing_label_in_combined;
                        
                        relabel_map_lock.lock();
                        int label_map_label_combined = km.insert_relabel(new_combined_label);
                        relabel_map_lock.unlock();
                        
                        label_map_lock.lock();
                        km.insert_label(km.label_map, label_map_label_combined);
                        label_map_lock.unlock();
                        vertex.set_data(label_map_label_combined);
                        logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label_combined << std::endl;
                    }
                }
            }
            
            // broadcast new label to neighbors by writing the value to the edges
            // write to the src_type if the vertex is the source vertex of the incident edge
            // write to the dst_type if the vertex is the destination vertex of the incident edge
            int label = vertex.get_data();
            if (gcontext.iteration == 0) {
                for(int i=0; i < vertex.num_inedges(); i++) {
                    graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                    type_label in_type = in_edge->get_data();
                    in_type.new_dst = label;
                    in_edge->set_data(in_type);
                }
                for (int i=0; i < vertex.num_outedges(); i++) {
                    graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                    type_label out_type = out_edge->get_data();
                    out_type.new_src = label;
                    out_edge->set_data(out_type);
                }
            } else {
                for(int i=0; i < vertex.num_inedges(); i++) {
                    graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                    type_label in_type = in_edge->get_data();
                    in_type.new_dst = label;
                    in_edge->set_data(in_type);
                    logstream(LOG_INFO) << "Updated in edges of " << vertex.id() << " to " << in_type.old_dst << std::endl;
                }
                for (int i=0; i < vertex.num_outedges(); i++) {
                    graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                    type_label out_type = out_edge->get_data();
                    out_type.new_src = label;
                    out_edge->set_data(out_type);
                    logstream(LOG_INFO) << "Updated out edges of " << vertex.id() << " to " << out_type.old_src << std::endl;
                }
            }
            /* Scheduler myself for next iteration */
            //gcontext.scheduler->add_task(vertex.id());
        }
    }
    
    /**
     * Called before an iteration starts.
     */
    void before_iteration(int iteration, graphchi_context &gcontext) {
    }
    
    /**
     * Called after an iteration has finished.
     */
    //For debugging purpose:
    void after_iteration(int iteration, graphchi_context &gcontext) {
//        logstream(LOG_INFO) << "After " << iteration << "th iteration:" << std::endl;
//        km.print_relabel_map();
//        km.print_label_map();
//        return;
    }
    
    /**
     * Called before an execution interval is started.
     */
    void before_exec_interval(vid_t window_st, vid_t window_en, graphchi_context &gcontext) {
    }
    
    /**
     * Called after an execution interval has finished.
     */
    void after_exec_interval(vid_t window_st, vid_t window_en, graphchi_context &gcontext) {
    }
    
};

struct VertexRelabel2 : public GraphChiProgram<VertexDataType, EdgeDataType> {

    std::mutex relabel_map_lock;
    std::mutex label_map_lock;
    /**
     *  Vertex update function.
     */
    void update(graphchi_vertex<VertexDataType, EdgeDataType> &vertex, graphchi_context &gcontext) {
        if (gcontext.iteration % 2 == 1) {
            for(int i=0; i < vertex.num_inedges(); i++) {
                graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                type_label in_type = in_edge->get_data();
                in_type.old_dst = in_type.new_dst;
                in_edge->set_data(in_type);
                logstream(LOG_INFO) << "Swapped in edges of " << vertex.id() << " to " << in_type.old_dst << std::endl;
            }
            for (int i=0; i < vertex.num_outedges(); i++) {
                graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                type_label out_type = out_edge->get_data();
                out_type.old_src = out_type.new_src;
                out_edge->set_data(out_type);
                logstream(LOG_INFO) << "Swapped out edges of " << vertex.id() << " to " << out_type.old_src << std::endl;
            }
        } else {
            if (gcontext.iteration == 0) {
                /* On first iteration, initialize vertex (and its edges). This is usually required, because
                 on each run, GraphChi will modify the data files. To start from scratch, it is easiest
                 do initialize the program in code. Alternatively, you can keep a copy of initial data files. */
                // First for each vertex, set its label as its w3ctype
                std::string vertex_label = "";
                // The value can be obtained from any outedge (from src_type) or in_edge from (dst_type)
                graphchi_edge<EdgeDataType> * outedge = vertex.random_outedge();
                //if the node has no outedge, we get a first inedge in the queue
                if (outedge == NULL) {
                    graphchi_edge<EdgeDataType> * inedge = vertex.inedge(0);
                    //get the dst_type from inedge
                    int vertex_int_label = inedge->get_data().new_dst;
                    std::string num_string;
                    std::stringstream out;
                    out << vertex_int_label;
                    vertex_label = out.str();
                } else {
                    //get the src_type from outedge
                    int vertex_int_label = outedge->get_data().new_src;
                    std::string num_string;
                    std::stringstream out;
                    out << vertex_int_label;
                    vertex_label = out.str();
                }
                //make sure vertex must have a valid string, not an empty string
                if (vertex_label != "") {
                    relabel_map_lock.lock();
                    int label_map_label = km.insert_relabel(vertex_label);
                    relabel_map_lock.unlock();
                    label_map_lock.lock();
                    km.insert_label(km.label_map_2, label_map_label);
                    label_map_lock.unlock();
                    vertex.set_data(label_map_label);
                    logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label << std::endl;
                } else {
                    logstream(LOG_FATAL) << "Invalid vertex_label in relabel_map. " << std::endl;
                    assert (vertex_label != "");
                }
            } else {
                if (VERSION == 0) {
                    std::vector<int> label_vec;
                    for(int i=0; i < vertex.num_inedges(); i++) {
                        graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                        int int_in_type = in_edge->get_data().old_src;
                        label_vec.push_back(int_in_type);
                        logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_type << " from in edges" << std::endl;
                        if (TAKEEDGELABEL == 1 && gcontext.iteration == 2) {
                            int int_in_edge_type = in_edge->get_data().edge;
                            label_vec.push_back(int_in_edge_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_edge_type << " (edge type) from in edges" << std::endl;
                        }
                    }
                    for (int i=0; i < vertex.num_outedges(); i++) {
                        graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                        int int_out_type = out_edge->get_data().old_dst;
                        label_vec.push_back(int_out_type);
                        logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_type << " from out edges" << std::endl;
                        if (TAKEEDGELABEL == 1 && gcontext.iteration == 2) {
                            int int_out_edge_type = out_edge->get_data().edge;
                            label_vec.push_back(int_out_edge_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_edge_type << " (edge type) from in edges" << std::endl;
                        }
                    }
                    std::sort(label_vec.begin(), label_vec.end());
                    int self_label = vertex.get_data();
                    label_vec.push_back(self_label);
                    
                    std::string new_label = "";
                    std::string first_num_string;
                    std::stringstream first_out;
                    first_out << *(label_vec.end() - 1);
                    first_num_string = first_out.str();
                    new_label += first_num_string;
                    new_label += ",";
                    
                    for (std::vector<int>::iterator it = label_vec.begin(); it != label_vec.end() - 1; ++it) {
                        std::string num_string;
                        std::stringstream out;
                        out << *it;
                        num_string = out.str();
                        new_label += num_string + " ";
                    }
                    
                    relabel_map_lock.lock();
                    int label_map_label = km.insert_relabel(new_label);
                    relabel_map_lock.unlock();
                    label_map_lock.lock();
                    km.insert_label(km.label_map_2, label_map_label);
                    label_map_lock.unlock();
                    vertex.set_data(label_map_label);
                    logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label << std::endl;
                }
                if (VERSION == 1) {
                    std::vector<int> incoming_label_vec;
                    std::vector<int> outgoing_label_vec;
                    for(int i=0; i < vertex.num_inedges(); i++) {
                        graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                        int int_in_type = in_edge->get_data().old_src;
                        incoming_label_vec.push_back(int_in_type);
                        logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_type << " from in edges" << std::endl;
                        if (TAKEEDGELABEL == 1 && gcontext.iteration == 2) {
                            int int_in_edge_type = in_edge->get_data().edge;
                            incoming_label_vec.push_back(int_in_edge_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_edge_type << " (edge type) from in edges" << std::endl;
                        }
                    }
                    for (int i=0; i < vertex.num_outedges(); i++) {
                        graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                        int int_out_type = out_edge->get_data().old_dst;
                        outgoing_label_vec.push_back(int_out_type);
                        logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_type << " from out edges" << std::endl;
                        if (TAKEEDGELABEL == 1 && gcontext.iteration == 2) {
                            int int_out_edge_type = out_edge->get_data().edge;
                            outgoing_label_vec.push_back(int_out_edge_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_edge_type << " (edge type) from in edges" << std::endl;
                        }
                    }
                    std::sort(incoming_label_vec.begin(), incoming_label_vec.end());
                    std::sort(outgoing_label_vec.begin(), outgoing_label_vec.end());
                    
                    int self_label = vertex.get_data();
                    incoming_label_vec.push_back(self_label);
                    outgoing_label_vec.push_back(self_label);
                    
                    std::string new_incoming_label = "";
                    std::string first_incoming_num_string;
                    std::stringstream first_incoming_out;
                    first_incoming_out << *(incoming_label_vec.end() - 1);
                    first_incoming_num_string = first_incoming_out.str();
                    new_incoming_label += first_incoming_num_string;
                    new_incoming_label += ",";
                    
                    for (std::vector<int>::iterator it = incoming_label_vec.begin(); it != incoming_label_vec.end() - 1; ++it) {
                        std::string incoming_num_string;
                        std::stringstream incoming_out;
                        incoming_out << *it;
                        incoming_num_string = incoming_out.str();
                        new_incoming_label += incoming_num_string + " ";
                    }
                    
                    std::string new_outgoing_label = "";
                    std::string first_outgoing_num_string;
                    std::stringstream first_outgoing_out;
                    first_outgoing_out << *(outgoing_label_vec.end() - 1);
                    first_outgoing_num_string = first_outgoing_out.str();
                    new_outgoing_label += first_outgoing_num_string;
                    new_outgoing_label += ",";
                    
                    for (std::vector<int>::iterator it = outgoing_label_vec.begin(); it != outgoing_label_vec.end() - 1; ++it) {
                        std::string outgoing_num_string;
                        std::stringstream outgoing_out;
                        outgoing_out << *it;
                        outgoing_num_string = outgoing_out.str();
                        new_outgoing_label += outgoing_num_string + " ";
                    }
                    
                    relabel_map_lock.lock();
                    int label_map_label_incoming = km.insert_relabel(new_incoming_label);
                    int label_map_label_outgoing = km.insert_relabel(new_outgoing_label);
                    relabel_map_lock.unlock();
                    
                    std::string new_combined_label = ""; //label_map_label_incoming,label_map_label_outgoing
                    std::string incoming_label_in_combined;
                    std::stringstream incoming_label_in_combined_out;
                    incoming_label_in_combined_out << label_map_label_incoming;
                    incoming_label_in_combined = incoming_label_in_combined_out.str();
                    new_combined_label += incoming_label_in_combined;
                    new_combined_label += ",";
                    std::string outgoing_label_in_combined;
                    std::stringstream outgoing_label_in_combined_out;
                    outgoing_label_in_combined_out << label_map_label_outgoing;
                    outgoing_label_in_combined = outgoing_label_in_combined_out.str();
                    new_combined_label += outgoing_label_in_combined;
                    
                    relabel_map_lock.lock();
                    int label_map_label_combined = km.insert_relabel(new_combined_label);
                    relabel_map_lock.unlock();
                    
                    label_map_lock.lock();
                    km.insert_label(km.label_map_2, label_map_label_combined);
                    label_map_lock.unlock();
                    vertex.set_data(label_map_label_combined);
                    logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label_combined << std::endl;
                }
                if (VERSION == 2) {
                    if (gcontext.iteration == 2) {
                        std::vector<std::pair<int, int>> incoming_pair_label_vec;
                        std::vector<std::pair<int, int>> outgoing_pair_label_vec;
                        for(int i=0; i < vertex.num_inedges(); i++) {
                            graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                            int int_in_type = in_edge->get_data().old_src;
                            int int_in_edge_type = in_edge->get_data().edge;
                            std::pair<int, int> pair_label_in (int_in_type, int_in_edge_type);
                            incoming_pair_label_vec.push_back(pair_label_in);
                        }
                        for (int i=0; i < vertex.num_outedges(); i++) {
                            graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                            int int_out_type = out_edge->get_data().old_dst;
                            int int_out_edge_type = out_edge->get_data().edge;
                            std::pair<int, int> pair_label_out (int_out_type, int_out_edge_type);
                            outgoing_pair_label_vec.push_back(pair_label_out);
                        }
                        struct {
                            bool operator()(std::pair<int, int> a, std::pair<int, int> b) {
                                return a.first < b.first;
                            }
                        } comparePair;
                        
                        std::sort(incoming_pair_label_vec.begin(), incoming_pair_label_vec.end(), comparePair);
                        std::sort(outgoing_pair_label_vec.begin(), outgoing_pair_label_vec.end(), comparePair);
                        
                        std::string new_incoming_label = "";
                        std::string first_incoming_num_string;
                        std::stringstream first_incoming_out;
                        first_incoming_out << vertex.get_data();
                        first_incoming_num_string = first_incoming_out.str();
                        new_incoming_label += first_incoming_num_string;
                        new_incoming_label += ",";
                        
                        for (std::vector<std::pair<int, int>>::iterator it = incoming_pair_label_vec.begin(); it != incoming_pair_label_vec.end(); ++it) {
                            std::string incoming_num_string_first;
                            std::stringstream incoming_out_first;
                            incoming_out_first << it->first;
                            incoming_num_string_first = incoming_out_first.str();
                            new_incoming_label += incoming_num_string_first + " ";
                            std::string incoming_num_string_second;
                            std::stringstream incoming_out_second;
                            incoming_out_second << it->second;
                            incoming_num_string_second = incoming_out_second.str();
                            new_incoming_label += incoming_num_string_second + " ";
                        }
                        
                        std::string new_outgoing_label = "";
                        std::string first_outgoing_num_string;
                        std::stringstream first_outgoing_out;
                        first_outgoing_out << vertex.get_data();
                        first_outgoing_num_string = first_outgoing_out.str();
                        new_outgoing_label += first_outgoing_num_string;
                        new_outgoing_label += ",";
                        
                        for (std::vector<std::pair<int, int>>::iterator it = outgoing_pair_label_vec.begin(); it != outgoing_pair_label_vec.end(); ++it) {
                            std::string outgoing_num_string_first;
                            std::stringstream outgoing_out_first;
                            outgoing_out_first << it->first;
                            outgoing_num_string_first = outgoing_out_first.str();
                            new_incoming_label += outgoing_num_string_first + " ";
                            std::string outgoing_num_string_second;
                            std::stringstream outgoing_out_second;
                            outgoing_out_second << it->second;
                            outgoing_num_string_second = outgoing_out_second.str();
                            new_incoming_label += outgoing_num_string_second + " ";
                        }
                        
                        relabel_map_lock.lock();
                        int label_map_label_incoming = km.insert_relabel(new_incoming_label);
                        int label_map_label_outgoing = km.insert_relabel(new_outgoing_label);
                        relabel_map_lock.unlock();
                        
                        std::string new_combined_label = ""; //label_map_label_incoming,label_map_label_outgoing
                        std::string incoming_label_in_combined;
                        std::stringstream incoming_label_in_combined_out;
                        incoming_label_in_combined_out << label_map_label_incoming;
                        incoming_label_in_combined = incoming_label_in_combined_out.str();
                        new_combined_label += incoming_label_in_combined;
                        new_combined_label += ",";
                        std::string outgoing_label_in_combined;
                        std::stringstream outgoing_label_in_combined_out;
                        outgoing_label_in_combined_out << label_map_label_outgoing;
                        outgoing_label_in_combined = outgoing_label_in_combined_out.str();
                        new_combined_label += outgoing_label_in_combined;
                        
                        relabel_map_lock.lock();
                        int label_map_label_combined = km.insert_relabel(new_combined_label);
                        relabel_map_lock.unlock();
                        
                        label_map_lock.lock();
                        km.insert_label(km.label_map_2, label_map_label_combined);
                        label_map_lock.unlock();
                        vertex.set_data(label_map_label_combined);
                        logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label_combined << std::endl;
                    } else {
                        std::vector<int> incoming_label_vec;
                        std::vector<int> outgoing_label_vec;
                        for(int i=0; i < vertex.num_inedges(); i++) {
                            graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                            int int_in_type = in_edge->get_data().old_src;
                            incoming_label_vec.push_back(int_in_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_in_type << " from in edges" << std::endl;
                        }
                        for (int i=0; i < vertex.num_outedges(); i++) {
                            graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                            int int_out_type = out_edge->get_data().old_dst;
                            outgoing_label_vec.push_back(int_out_type);
                            logstream(LOG_INFO) << "Vertex " << vertex.id() << " getting " << int_out_type << " from out edges" << std::endl;
                        }
                        std::sort(incoming_label_vec.begin(), incoming_label_vec.end());
                        std::sort(outgoing_label_vec.begin(), outgoing_label_vec.end());
                        
                        int self_label = vertex.get_data();
                        incoming_label_vec.push_back(self_label);
                        outgoing_label_vec.push_back(self_label);
                        
                        std::string new_incoming_label = "";
                        std::string first_incoming_num_string;
                        std::stringstream first_incoming_out;
                        first_incoming_out << *(incoming_label_vec.end() - 1);
                        first_incoming_num_string = first_incoming_out.str();
                        new_incoming_label += first_incoming_num_string;
                        new_incoming_label += ",";
                        
                        for (std::vector<int>::iterator it = incoming_label_vec.begin(); it != incoming_label_vec.end() - 1; ++it) {
                            std::string incoming_num_string;
                            std::stringstream incoming_out;
                            incoming_out << *it;
                            incoming_num_string = incoming_out.str();
                            new_incoming_label += incoming_num_string + " ";
                        }
                        
                        std::string new_outgoing_label = "";
                        std::string first_outgoing_num_string;
                        std::stringstream first_outgoing_out;
                        first_outgoing_out << *(outgoing_label_vec.end() - 1);
                        first_outgoing_num_string = first_outgoing_out.str();
                        new_outgoing_label += first_outgoing_num_string;
                        new_outgoing_label += ",";
                        
                        for (std::vector<int>::iterator it = outgoing_label_vec.begin(); it != outgoing_label_vec.end() - 1; ++it) {
                            std::string outgoing_num_string;
                            std::stringstream outgoing_out;
                            outgoing_out << *it;
                            outgoing_num_string = outgoing_out.str();
                            new_outgoing_label += outgoing_num_string + " ";
                        }
                        
                        relabel_map_lock.lock();
                        int label_map_label_incoming = km.insert_relabel(new_incoming_label);
                        int label_map_label_outgoing = km.insert_relabel(new_outgoing_label);
                        relabel_map_lock.unlock();
                        
                        std::string new_combined_label = ""; //label_map_label_incoming,label_map_label_outgoing
                        std::string incoming_label_in_combined;
                        std::stringstream incoming_label_in_combined_out;
                        incoming_label_in_combined_out << label_map_label_incoming;
                        incoming_label_in_combined = incoming_label_in_combined_out.str();
                        new_combined_label += incoming_label_in_combined;
                        new_combined_label += ",";
                        std::string outgoing_label_in_combined;
                        std::stringstream outgoing_label_in_combined_out;
                        outgoing_label_in_combined_out << label_map_label_outgoing;
                        outgoing_label_in_combined = outgoing_label_in_combined_out.str();
                        new_combined_label += outgoing_label_in_combined;
                        
                        relabel_map_lock.lock();
                        int label_map_label_combined = km.insert_relabel(new_combined_label);
                        relabel_map_lock.unlock();
                        
                        label_map_lock.lock();
                        km.insert_label(km.label_map_2, label_map_label_combined);
                        label_map_lock.unlock();
                        vertex.set_data(label_map_label_combined);
                        logstream(LOG_INFO) << "The value of label " << vertex.id() << " is: " << label_map_label_combined << std::endl;
                    }
                }
            }
            
            // broadcast new label to neighbors by writing the value to the edges
            // write to the src_type if the vertex is the source vertex of the incident edge
            // write to the dst_type if the vertex is the destination vertex of the incident edge
            int label = vertex.get_data();
            if (gcontext.iteration == 0) {
                for(int i=0; i < vertex.num_inedges(); i++) {
                    graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                    type_label in_type = in_edge->get_data();
                    in_type.new_dst = label;
                    in_edge->set_data(in_type);
                }
                for (int i=0; i < vertex.num_outedges(); i++) {
                    graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                    type_label out_type = out_edge->get_data();
                    out_type.new_src = label;
                    out_edge->set_data(out_type);
                }
            } else {
                for(int i=0; i < vertex.num_inedges(); i++) {
                    graphchi_edge<EdgeDataType> * in_edge = vertex.inedge(i);
                    type_label in_type = in_edge->get_data();
                    in_type.new_dst = label;
                    in_edge->set_data(in_type);
                    logstream(LOG_INFO) << "Updated in edges of " << vertex.id() << " to " << in_type.old_dst << std::endl;
                }
                for (int i=0; i < vertex.num_outedges(); i++) {
                    graphchi_edge<EdgeDataType> * out_edge = vertex.outedge(i);
                    type_label out_type = out_edge->get_data();
                    out_type.new_src = label;
                    out_edge->set_data(out_type);
                    logstream(LOG_INFO) << "Updated out edges of " << vertex.id() << " to " << out_type.old_src << std::endl;
                }
            }
        }
    }
    
    /**
     * Called before an iteration starts.
     */
    void before_iteration(int iteration, graphchi_context &gcontext) {
    }
    
    /**
     * Called after an iteration has finished.
     */
    //For debugging purpose:
    void after_iteration(int iteration, graphchi_context &gcontext) {
        //        logstream(LOG_INFO) << "After " << iteration << "th iteration:" << std::endl;
        //        km.print_relabel_map();
        //        km.print_label_map();
        //        return;
    }
    
    /**
     * Called before an execution interval is started.
     */
    void before_exec_interval(vid_t window_st, vid_t window_en, graphchi_context &gcontext) {
    }
    
    /**
     * Called after an execution interval has finished.
     */
    void after_exec_interval(vid_t window_st, vid_t window_en, graphchi_context &gcontext) {
    }
    
};

int main(int argc, const char ** argv) {
    /* GraphChi initialization will read the command line
     arguments and the configuration file. */
    graphchi_init(argc, argv);
    
    /* Metrics object for keeping track of performance counters
     and other information. Currently required. */
    metrics m("vertexlabel");
    
    /* Basic arguments for application */
    //First, we must know how many graphs will be used for computation:
    int num_graphs = get_option_int("ngraphs");
    //We then use the for loop to get all the filenames, and store them in an array
    std::string filenames[num_graphs] = {};
    for (int i = 0; i < num_graphs; i++) {
        std::string f_string;
        std::stringstream f_out;
        f_out << i;
        f_string = f_out.str();
        const std::string name_of_file = "file" + f_string;
        filenames[i] = get_option_string(name_of_file.c_str());
    }
    //*********Hardcode file names with hardcore preset number of files
    //std::string filename = get_option_string("file");  // Base filename
    //std::string filename2 = get_option_string("file2");
    //**************
    int niters           = get_option_int("niters", 10); // Number of iterations
    //bool scheduler       = get_option_int("scheduler", 0); // Whether to use selective scheduling
    //TODO: should I use selective scheduling?
    bool scheduler       = false;
    
    /* Detect the number of shards or preprocess an input to create them */
    //for each file, detect shards or preprocess an input to create them
    //put results in an array
    int nshards_arr[num_graphs] = {};
    for (int i = 0; i < num_graphs; i++) {
        nshards_arr[i] = convert_if_notexists<EdgeDataType>(filenames[i], get_option_string("nshards", "auto"));
    }
    //***************hardcoded with preset number of files
//    int nshards          = convert_if_notexists<EdgeDataType>(filename,
//                                                             get_option_string("nshards", "auto"));
//    
//    int nshards2         = convert_if_notexists<EdgeDataType>(filename2,
//                                                              get_option_string("nshards", "auto"));
    //**********************
    /* Run */
    //TODO:This part can be made more efficient, as the principle graph (those in VertexRelabel) only needs to be run once
    //modify this code and resetMaps code to streamline
    for (int i = 0 ; i < num_graphs; i++) {
        for (int j = 0; j < num_graphs - i; j++) {
            VertexRelabel program;
            graphchi_engine<VertexDataType, EdgeDataType> engine(filenames[i], nshards_arr[i], scheduler, m);
            engine.run(program, niters);
            
            VertexRelabel2 program2;
            graphchi_engine<VertexDataType, EdgeDataType> engine2(filenames[i+j], nshards_arr[i+j], scheduler, m);
            engine2.run(program2, niters);
            
            //calculate the kernel value between two graphs
            int k_value = km.calculate_kernel(km.label_map, km.label_map_2);
            //reset the maps for next iteration
            km.resetMaps(0);
            //push the kernel value into the global kv vector
            kv.push_back(k_value);
            //print kernel value of two graphs:
            logstream(LOG_INFO) << "WL Kernel value is: " << k_value << std::endl;
        }
    }
    //print kv
    std::cout << "Kernel vector values: ";
    for(size_t i = 0; i < kv.size(); i++) {
        std::cout << kv[i] << " ";
    }
    std::cout << std::endl;
    
    //produce normalized kernel value for each graph
    double normalized_kv[num_graphs] = {};
    //sum of multiplication
    if (METRIC == 0) {
        for (int i = 0; i < num_graphs; i++) {
            double total = 0.0;
            for (int j = 0; j < num_graphs; j++) {
                if (i != j) {
                    int self = kv[i*num_graphs-i*(i-1)/2+j-i];
                    int base1 = kv[i*num_graphs-i*(i-1)/2+i-i];
                    int base2 = kv[j*num_graphs-j*(j-1)/2+j-j];
                    double normalized = (double) self/((double)base1*(double)base2);
                    total += normalized;
                }
            }
            normalized_kv[i] = total/num_graphs;
        }
    } else if (METRIC == 1) {//geometric distance
        for (int i = 0; i < num_graphs; i++) {
            double total = 0.0;
            for (int j = 0; j < num_graphs; j++) {
                if (i != j) {
                    int dist = sqrt(kv[i*num_graphs-i*(i-1)/2+j-i]);
                    total += dist;
                }
            }
            normalized_kv[i] = total/(num_graphs - 1);
        }
    }
    //print normalize_kv
    std::cout << "Normalized kernel vector values: ";
    for(int i = 0; i < num_graphs; i++) {
        std::cout << normalized_kv[i] << " ";
    }
    std::cout << std::endl;
    
    //calculate mean and std of normalized_kv
    double sum = 0.0, mean, standardDeviation = 0.0;
    int i;
    for(i = 0; i < num_graphs; ++i){
        sum += normalized_kv[i];
    }
    mean = sum/num_graphs;
    for(i = 0; i < num_graphs; ++i)
        standardDeviation += pow(normalized_kv[i] - mean, 2);
    standardDeviation = sqrt(standardDeviation / num_graphs);
    std::cout << "Mean: " << mean << std::endl;
    std::cout << "STD: " << standardDeviation << std::endl;
    
    //Determine instances with unusual behavior, and then regenerate the mean and STD without them as part of the profile
    double two_stds_upper_bound = mean + 2 * standardDeviation;
    double two_stds_lower_bound = mean - 2 * standardDeviation;
    std::cout << "boundary is: " << two_stds_lower_bound << " - " << two_stds_upper_bound << std::endl;
    std::vector<double> good_normalized_kv;
    for(int i = 0; i < num_graphs; ++i) {
        if (normalized_kv[i] <= two_stds_upper_bound && normalized_kv[i] >= two_stds_lower_bound)
            good_normalized_kv.push_back(normalized_kv[i]);
    }
    
    double good_sum = 0.0, good_mean, good_standardDeviation = 0.0;
    for(std::vector<double>::iterator it = good_normalized_kv.begin(); it != good_normalized_kv.end(); it++){
        good_sum += *it;
    }
    good_mean = good_sum/good_normalized_kv.size();
    for(std::vector<double>::iterator it = good_normalized_kv.begin(); it != good_normalized_kv.end(); it++)
        good_standardDeviation += pow(*it - mean, 2);
    good_standardDeviation = sqrt(good_standardDeviation / good_normalized_kv.size());
    std::cout << "Good Mean: " << good_mean << std::endl;
    std::cout << "Good STD: " << good_standardDeviation << std::endl;
    
    
    //**********Hardcoded for two graphs only
//    VertexRelabel program;
//    graphchi_engine<VertexDataType, EdgeDataType> engine(filename, nshards, scheduler, m);
//    engine.run(program, niters);
//    
//    VertexRelabel2 program2;
//    graphchi_engine<VertexDataType, EdgeDataType> engine2(filename2, nshards2, scheduler, m);
//    engine2.run(program2, niters);
//    
//    //Print maps
//    km.print_relabel_map();
//    km.print_label_map(km.label_map);
//    km.print_label_map(km.label_map_2);
//    
//    //calculate the kernel value between two graphs
//    int k_value = km.calculate_kernel(km.label_map, km.label_map_2);
//    //print kernel value of two graphs:
//    logstream(LOG_INFO) << "WL Kernel value is: " << k_value << std::endl;
    //*******************************
    
    /* Report execution metrics */
    //metrics_report(m);
    return 0;
}
