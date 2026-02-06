#pragma once
#include <vector>
#include <iostream>
#include <stdexcept>
#include <utility>




/// @brief i->j edge is [i][j]
struct Graph{
    std::vector<std::vector<int>> matrix;
    int size;

    Graph():size(0),matrix(){}

    //Никаких проверок
    Graph(int n):size(n),matrix(std::vector<std::vector<int>>(n,std::vector<int>(n))){};
    //Никаких проверок
    Graph(const std::vector<std::vector<int>>& v):size(v.size()),matrix(v){
        if(v.size()>0 && v.size()!=v[0].size()){
            throw std::invalid_argument("Not quadratic matrix init");
        } 
    }



    void add_edge(int i,int j,int value=1){
        if(i<0 || j<0 || i>=size || j>=size){throw std::invalid_argument("Index out of bounds");}

        matrix[i][j]=value;
        matrix[j][i]=value;
    };

    void remove_edge(int i,int j){
        if(i<0 || j<0 || i>=size || j>=size){throw std::invalid_argument("Index out of bounds");}
        matrix[i][j]=0;
        matrix[j][i]=0;
    };

    void add_vert(){
        for(int i=0;i<size;i++){
            matrix[i].push_back(0);
        }
        matrix.push_back(std::vector<int>(size+1));
        size++;
    };

    void swap_vert(int i,int j){
        if(i<0 || j<0 || i>=size || j>=size){throw std::invalid_argument("Index out of bounds");}
        if(i==j){return;}
        for(int k=0;k<size;k++){
            std::swap(matrix[k][i],matrix[k][j]);
        }
        for(int k=0;k<size;k++){
            std::swap(matrix[i][k],matrix[j][k]);
        }

    };
    


    //removes last vert
    void pop_vert(){
        if(size<1){return;}
        for(int i=0;i<size-1;i++){
            matrix[i].pop_back();
        }
        matrix.pop_back();
        size--;
    };

    void remove_vert(int i){
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        if(i!=size-1) swap_vert(i,size-1);
        pop_vert();
    };

    int degree(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count++;
        }
        return count;
    }

    int degree_in(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count++;
        }
        return count;
    }

    int degree_out(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[k][i]!=0) count++;
        }
        return count;
    }

    int degree_oriented(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count++;
        }
        for(int k=0;k<size;k++){
            if(matrix[k][i]!=0) count++;
        }
        return count;
    }

     int weight(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count+=matrix[i][k];
        }
        return count;
    }

    int weight_in(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count+=matrix[i][k];
        }
        return count;
    }

    int weight_out(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count+=matrix[i][k];
        }
        return count;
    }

    int weight_oriented(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count+=matrix[i][k];
        }
        return count;
    }


    void print() const {
        for(int i=0;i<size;i++){
            for(int j=0;j<size;j++){
                std::cout<<matrix[i][j]<<' ';
            }
            std::cout<<"\n";
        }
    }
};
