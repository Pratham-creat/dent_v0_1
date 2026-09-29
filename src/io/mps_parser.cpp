#include "dent/io/mps_parser.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dent { namespace {
struct Row { std::string name; char type='E'; };
struct Bound { std::string column, type; double value=0.0; };
struct Entry { std::string row, column; double value=0.0; };
struct Range { std::string row; double value=0.0; };

std::vector<std::string> tokens(const std::string& s) {
    std::istringstream in(s); std::vector<std::string> v; std::string x;
    while(in>>x) v.push_back(x); return v;
}
std::string upper(std::string s) {
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
    return s;
}
double number(const std::string& s,const std::string& where) {
    try { std::size_t n=0; double v=std::stod(s,&n); if(n!=s.size()) throw std::runtime_error(""); return v; }
    catch(...) { throw std::runtime_error("Invalid numeric value '"+s+"' in "+where); }
}
}} // namespace dent::<anonymous>

namespace dent {
Problem MPSParser::parse_file(const std::string& filename) {
    std::ifstream input(filename);
    if(!input) throw std::runtime_error("Could not open MPS file: "+filename);
    enum class Section { None,Rows,Columns,Rhs,Ranges,Bounds,ObjSense,QuadObj,QMatrix };
    Section section=Section::None;
    std::vector<Row> rows; std::vector<Entry> linear,quadratic; std::vector<Range> ranges; std::vector<Bound> bounds;
    std::unordered_set<std::string> integer_columns;
    std::string objective_row,line; ObjectiveSense objective_sense=ObjectiveSense::Minimize; bool integer_mode=false;

    auto section_of=[](const std::string& s){
        const std::string u=upper(s);
        if(u=="ROWS") return Section::Rows; if(u=="COLUMNS") return Section::Columns; if(u=="RHS") return Section::Rhs;
        if(u=="RANGES") return Section::Ranges; if(u=="BOUNDS") return Section::Bounds; if(u=="OBJSENSE") return Section::ObjSense;
        if(u=="QUADOBJ") return Section::QuadObj; if(u=="QMATRIX") return Section::QMatrix; return Section::None;
    };

    while(std::getline(input,line)) {
        if(!line.empty() && line[0]=='*') continue;
        auto t=tokens(line); if(t.empty()) continue;
        Section s=section_of(t[0]); if(s!=Section::None){section=s;continue;}
        const std::string first=upper(t[0]); if(first=="NAME"||first=="ENDATA") continue;
        switch(section) {
        case Section::Rows:
            if(t.size()<2) throw std::runtime_error("Malformed ROWS record.");
            if(t[0]!="N"&&t[0]!="L"&&t[0]!="G"&&t[0]!="E") throw std::runtime_error("Unknown MPS row type: "+t[0]);
            rows.push_back({t[1],t[0][0]}); if(t[0]=="N"&&objective_row.empty()) objective_row=t[1]; break;
        case Section::Columns:
            if(t.size()>=2&&upper(t[1])=="MARKER"){for(const auto& x:t){if(upper(x)=="INTORG")integer_mode=true;if(upper(x)=="INTEND")integer_mode=false;}break;}
            if(t.size()<3) throw std::runtime_error("Malformed COLUMNS record.");
            if(integer_mode) integer_columns.insert(t[0]);
            linear.push_back({t[1],t[0],number(t[2],"COLUMNS")});
            if(t.size()>=5) linear.push_back({t[3],t[0],number(t[4],"COLUMNS")});
            break;
        case Section::Rhs:
            if(t.size()<3) throw std::runtime_error("Malformed RHS record.");
            linear.push_back({t[1],"",number(t[2],"RHS")}); if(t.size()>=5) linear.push_back({t[3],"",number(t[4],"RHS")}); break;
        case Section::Ranges:
            if(t.size()<3) throw std::runtime_error("Malformed RANGES record.");
            ranges.push_back({t[1],number(t[2],"RANGES")}); if(t.size()>=5) ranges.push_back({t[3],number(t[4],"RANGES")}); break;
        case Section::Bounds:
            if(t.size()<3) throw std::runtime_error("Malformed BOUNDS record.");
            bounds.push_back({t[2],upper(t[0]),t.size()>=4?number(t[3],"BOUNDS"):0.0}); break;
        case Section::ObjSense:
            if(upper(t[0])=="MAX") objective_sense=ObjectiveSense::Maximize;
            else if(upper(t[0])=="MIN") objective_sense=ObjectiveSense::Minimize;
            else throw std::runtime_error("OBJSENSE must be MIN or MAX."); break;
        case Section::QuadObj:
        case Section::QMatrix:
            if(t.size()<3) throw std::runtime_error("Malformed quadratic record.");
            quadratic.push_back({t[1],t[0],number(t[2],"quadratic section")}); if(t.size()>=5) quadratic.push_back({t[3],t[0],number(t[4],"quadratic section")}); break;
        case Section::None: break;
        }
    }
    if(rows.empty()) throw std::runtime_error("MPS file contains no ROWS section.");

    std::unordered_map<std::string,int> row_index,column_index; std::vector<Row> model_rows; std::vector<std::string> columns;
    for(const auto& r:rows){if(r.type=='N'){if(objective_row.empty())objective_row=r.name;}else{row_index[r.name]=static_cast<int>(model_rows.size());model_rows.push_back(r);}}
    auto add_column=[&](const std::string& n){if(!column_index.count(n)){column_index[n]=static_cast<int>(columns.size());columns.push_back(n);}};
    for(const auto& e:linear) if(!e.column.empty()) add_column(e.column);
    for(const auto& b:bounds) add_column(b.column);
    for(const auto& q:quadratic){add_column(q.row);add_column(q.column);}

    Problem problem(objective_sense);
    for(const auto& n:columns) problem.add_variable(n);
    std::unordered_map<std::string,double> rhs;
    for(const auto& e:linear) if(e.column.empty()) rhs[e.row]=e.value;
    for(const auto& r:model_rows){ConstraintSense s=ConstraintSense::Equal;if(r.type=='L')s=ConstraintSense::LessEqual;if(r.type=='G')s=ConstraintSense::GreaterEqual;problem.add_constraint(r.name,s,rhs.count(r.name)?rhs[r.name]:0.0);}

    for(const auto& e:linear) if(!e.column.empty()){
        const int j=column_index.at(e.column);
        if(e.row==objective_row) problem.set_objective_coefficient(j,problem.objective()[j]+e.value);
        else if(row_index.count(e.row)){const int i=row_index.at(e.row);problem.set_constraint_coefficient(i,j,problem.matrix()[i][j]+e.value);}
    }
    for(const auto& n:integer_columns) if(column_index.count(n)) problem.set_variable_type(column_index.at(n),VariableType::Integer);
    for(const auto& q:quadratic){const int i=column_index.at(q.row),j=column_index.at(q.column);problem.set_quadratic_coefficient(i,j,q.value);problem.set_quadratic_coefficient(j,i,q.value);}

    for(const auto& r:ranges){
        auto it=row_index.find(r.row); if(it==row_index.end()) continue;
        const int i=it->second; const auto base=problem.constraints()[i]; const double d=r.value;
        if(base.sense==ConstraintSense::LessEqual){
            problem.add_constraint(base.name+"_range_lower",ConstraintSense::GreaterEqual,base.rhs-std::abs(d));
        } else if(base.sense==ConstraintSense::GreaterEqual){
            problem.add_constraint(base.name+"_range_upper",ConstraintSense::LessEqual,base.rhs+std::abs(d));
        } else if(d>=0.0){
            problem.set_constraint_sense(i,ConstraintSense::GreaterEqual);
            problem.add_constraint(base.name+"_range_upper",ConstraintSense::LessEqual,base.rhs+d);
        } else {
            problem.set_constraint_rhs(i,base.rhs+d);
            problem.set_constraint_sense(i,ConstraintSense::GreaterEqual);
            problem.add_constraint(base.name+"_range_upper",ConstraintSense::LessEqual,base.rhs);
        }
        const int extra=static_cast<int>(problem.constraints().size())-1;
        for(int j=0;j<static_cast<int>(columns.size());++j) problem.set_constraint_coefficient(extra,j,problem.matrix()[i][j]);
    }

    for(const auto& b:bounds){
        const int j=column_index.at(b.column), k=upper(b.type);
        if(k=="BV") problem.set_variable_type(j,VariableType::Binary);
        else if(k=="FX") problem.set_variable_bounds(j,b.value,b.value);
        else if(k=="LO") problem.set_variable_bounds(j,b.value,0.0);
        else if(k=="UP") problem.set_variable_bounds(j,0.0,b.value);
        else if(k=="UI"){problem.set_variable_type(j,VariableType::Integer);problem.set_variable_bounds(j,0.0,b.value);}
        else if(k=="LI"){problem.set_variable_type(j,VariableType::Integer);problem.set_variable_bounds(j,b.value,0.0);}
        else if(k=="PL") problem.set_variable_bounds(j,0.0,0.0);
        else if(k=="FR"||k=="MI") throw std::runtime_error("MPS bound type "+k+" requires a free/negative-infinite lower bound; DENT simplex currently requires nonnegative lower bounds.");
        else throw std::runtime_error("Unsupported MPS bound type: "+k);
    }
    return problem;
}
} // namespace dent
