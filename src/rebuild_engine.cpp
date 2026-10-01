// Independent R4 evaluator, written for the 2026-09-29 rebuild.
// C++17 standard library only. No legacy implementation or results are loaded.
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace r4 {
using U64 = std::uint64_t;
using Perm = std::vector<int>;
using Bits = std::vector<U64>;
struct Comparator { int a, b, first, direction; };
struct Op { int a, b, out; bool is_min; };
struct Net {
    int n = 0, output = 0, wires = 0, active_comparators = 0;
    std::vector<Comparator> comparators;
    std::vector<Op> ops;
    std::vector<int> active_inputs;
};
struct Parser {
    const std::string& text; std::size_t pos = 0;
    void space() { while (pos < text.size() && (text[pos]==' ' || text[pos]=='\n' || text[pos]=='\r' || text[pos]=='\t')) ++pos; }
    void token(char c) { space(); if (pos == text.size() || text[pos++] != c) throw std::runtime_error("invalid netlist punctuation"); }
    int integer() {
        space(); if (pos == text.size() || text[pos]<'0' || text[pos]>'9') throw std::runtime_error("expected nonnegative integer");
        int value = 0;
        while (pos < text.size() && text[pos]>='0' && text[pos]<='9') {
            const int d=text[pos++]-'0';
            if (value > (std::numeric_limits<int>::max()-d)/10) throw std::runtime_error("integer overflow");
            value=value*10+d;
        }
        return value;
    }
};
Net parse(const std::string& text) {
    if (text.size()>1024*1024) throw std::runtime_error("netlist exceeds 1 MiB parser limit");
    Parser p{text}; p.token('{'); std::array<int,7> h{};
    for (int i=0;i<7;++i) { h[i]=p.integer(); if(i<6) p.token(','); } p.token('}');
    if ((h[0]!=9 && h[0]!=25) || h[1]!=1 || h[2]>4096 || h[3]!=1 || h[4]!=2 || h[5]!=2)
        throw std::runtime_error("unsupported .cha header (requires 9/25 inputs, one output, one row, two binary functions)");
    Net net; net.n=h[0]; net.wires=net.n+2*h[2];
    for(int k=0;k<h[2];++k) {
        p.token('(');p.token('[');int first=p.integer();p.token(',');int second=p.integer();p.token(']');
        int a=p.integer();p.token(',');int b=p.integer();p.token(',');int direction=p.integer();p.token(')');
        if(first!=net.n+2*k || second!=first+1) throw std::runtime_error("nonsequential or duplicate comparator output");
        if(a>=first || b>=first) throw std::runtime_error("forward or invalid comparator reference");
        if(direction!=1 && direction!=2) throw std::runtime_error("invalid comparator direction");
        net.comparators.push_back({a,b,first,direction});
    }
    p.token('(');net.output=p.integer();p.token(')');p.space();
    if(p.pos!=text.size()) throw std::runtime_error("trailing netlist content");
    if(net.output>=net.wires) throw std::runtime_error("invalid final output");
    std::vector<bool> active(net.wires,false);active[net.output]=true;
    for(auto it=net.comparators.rbegin();it!=net.comparators.rend();++it)
        if(active[it->first] || active[it->first+1]) {active[it->a]=active[it->b]=true;++net.active_comparators;}
    for(const auto& c:net.comparators) for(int k=0;k<2;++k)
        if(active[c.first+k]) net.ops.push_back({c.a,c.b,c.first+k,(c.direction==1)==(k==0)});
    for(int i=0;i<net.n;++i) if(active[i]) net.active_inputs.push_back(i);
    return net;
}
Net load_net(const std::string& path) {
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot open netlist");
    std::ostringstream s;s<<f.rdbuf();if(f.bad())throw std::runtime_error("failed reading netlist");return parse(s.str());
}
Perm identity(int n) {Perm p(n);std::iota(p.begin(),p.end(),0);return p;}
void check_perm(const Perm& p,int n) {
    if(static_cast<int>(p.size())!=n)throw std::runtime_error("mapping has wrong length");
    std::vector<bool> seen(n,false);
    for(int x:p) {if(x<0 || x>=n || seen[x])throw std::runtime_error("mapping is not a permutation");seen[x]=true;}
}
Perm parse_perm(const std::string& s,int n) {
    Parser reader{s};Perm p;for(int i=0;i<n;++i) {p.push_back(reader.integer());if(i<n-1)reader.token(',');}
    reader.space();if(reader.pos!=s.size())throw std::runtime_error("trailing mapping content");check_perm(p,n);return p;
}
Perm compose(const Perm& a,const Perm& b) {Perm c(a.size());for(std::size_t i=0;i<c.size();++i)c[i]=a[b[i]];return c;}
const std::array<const char*,8> names={"identity","r90","r180","r270","mirror","r90_mirror","r180_mirror","r270_mirror"};
std::vector<Perm> d4(int n) {
    const int side=(n==9?3:(n==25?5:0));if(!side)throw std::runtime_error("D4 supports 3x3 or 5x5 only");
    std::vector<Perm> group;
    // Each map is the source-coordinate pullback x'_j = x_{g(j)}.
    // r90: (row,column) -> (column,side-1-row). mirror: left-right.
    for(int mirrored=0;mirrored<2;++mirrored)for(int rotation=0;rotation<4;++rotation) {
        Perm g(n);for(int i=0;i<n;++i) {int r=i/side,c=i%side;if(mirrored)c=side-1-c;
            for(int k=0;k<rotation;++k){int next=c;c=side-1-r;r=next;}g[i]=r*side+c;
        }check_perm(g,n);group.push_back(g);
    }
    std::set<Perm> members(group.begin(),group.end());
    if(members.size()!=8 || group[0]!=identity(n))throw std::runtime_error("D4 distinctness/identity failure");
    for(const auto& a:group) {bool inverse=false;for(const auto& b:group) {
        auto c=compose(a,b);if(!members.count(c))throw std::runtime_error("D4 closure failure");if(c==group[0])inverse=true;
    }if(!inverse)throw std::runtime_error("D4 inverse failure");}
    return group;
}
static constexpr std::array<U64,6> low_masks={
    UINT64_C(0xaaaaaaaaaaaaaaaa),UINT64_C(0xcccccccccccccccc),UINT64_C(0xf0f0f0f0f0f0f0f0),
    UINT64_C(0xff00ff00ff00ff00),UINT64_C(0xffff0000ffff0000),UINT64_C(0xffffffff00000000)};
Bits truth(const Net& net,const Perm& sources) {
    check_perm(sources,net.n);const std::size_t words=(std::size_t{1}<<net.n)/64;
    Bits result(words);std::vector<U64> wires(net.wires);
    for(std::size_t word=0;word<words;++word) {
        for(int i:net.active_inputs) {const int source=sources[i];wires[i]=(source<6?low_masks[source]:U64{0}-U64((word>>(source-6))&1));}
        for(const auto& op:net.ops)wires[op.out]=op.is_min?(wires[op.a]&wires[op.b]):(wires[op.a]|wires[op.b]);
        result[word]=wires[net.output];
    }return result;
}
int scalar(const Net& net,const std::vector<int>& input) {
    if(static_cast<int>(input.size())!=net.n)throw std::runtime_error("scalar input length");
    std::vector<int> wires(net.wires);std::copy(input.begin(),input.end(),wires.begin());
    for(const auto& op:net.ops)wires[op.out]=op.is_min?std::min(wires[op.a],wires[op.b]):std::max(wires[op.a],wires[op.b]);
    return wires[net.output];
}
U64 pop(U64 x) {return static_cast<U64>(__builtin_popcountll(x));}
U64 bit(const Bits& t,U64 x) {return (t[x/64]>>(x%64))&1;}
struct Cofactors {U64 ones=0;std::vector<U64>A0,A1;};
Cofactors cofactors(const Bits& t,int n) {
    Cofactors c;c.A0.resize(n);c.A1.resize(n);for(U64 w:t)c.ones+=pop(w);
    for(int pin=0;pin<n;++pin) {
        for(std::size_t w=0;w<t.size();++w) {
            if(pin<6)c.A1[pin]+=pop(t[w]&low_masks[pin]);
            else if((w>>(pin-6))&1)c.A1[pin]+=pop(t[w]);
        }c.A0[pin]=c.ones-c.A1[pin];
    }return c;
}
struct Defect {U64 union_count=0;std::array<U64,8>each{};};
Defect defect(const Net& net,const Perm& p,const Bits& base,const std::vector<Perm>& group) {
    Defect d;Bits different(base.size(),0);
    for(int k=1;k<8;++k) {Bits transformed=truth(net,compose(group[k],p));
        for(std::size_t w=0;w<base.size();++w){U64 delta=base[w]^transformed[w];d.each[k]+=pop(delta);different[w]|=delta;}
    }for(U64 w:different)d.union_count+=pop(w);return d;
}
std::vector<U64> rank9(const Net& net,const Perm& p) {
    if(net.n!=9)throw std::runtime_error("rank9 requires nine inputs");check_perm(p,9);
    Perm order=identity(9);std::vector<int>wires(net.wires);std::vector<U64>histogram(9,0);
    // All 9! distinct numeric assignments, values 0..8, output rank indexed 1..9 in JSON.
    do {for(int i=0;i<9;++i)wires[i]=order[p[i]];
        for(const auto& op:net.ops)wires[op.out]=op.is_min?std::min(wires[op.a],wires[op.b]):std::max(wires[op.a],wires[op.b]);
        ++histogram.at(wires[net.output]);
    }while(std::next_permutation(order.begin(),order.end()));return histogram;
}
struct Orbit {std::vector<std::pair<int,U64>> words;int size=0;};
std::vector<Orbit> cube_orbits9(const std::vector<Perm>& group) {
    std::array<bool,512> seen{};std::vector<Orbit> orbits;
    for(int x=0;x<512;++x) {
      if(!seen[x]) {
        std::set<int> members;for(const auto& g:group){int y=0;for(int i=0;i<9;++i)y|=((x>>g[i])&1)<<i;members.insert(y);}
        Orbit orbit;std::map<int,U64> masks;for(int y:members){seen[y]=true;masks[y/64]|=U64{1}<<(y%64);++orbit.size;}
        for(auto pair:masks)orbit.words.push_back(pair);orbits.push_back(orbit);
      }
    }
    return orbits;
}
// The union defect marks every member of a nonconstant D4 orbit and no member
// of a constant orbit. This avoids separately evaluating seven transforms at 9! mappings.
U64 orbit_defect9(const std::array<U64,8>& mapped,const std::vector<Orbit>& orbits) {
    U64 count=0;for(const auto& orbit:orbits){bool has_zero=false,has_one=false;
        for(const auto& item:orbit.words){const U64 hit=mapped[item.first]&item.second;has_one|=hit!=0;has_zero|=hit!=item.second;if(has_zero&&has_one)break;}
        if(has_zero&&has_one)count+=orbit.size;
    }return count;
}
std::array<U64,8> permute_truth9(const Bits& original,const Perm& p) {
    check_perm(p,9);std::array<int,9> contribution{};for(int i=0;i<9;++i)contribution[p[i]]=1<<i;
    std::array<int,512> core{};std::array<U64,8> mapped{};
    if(bit(original,0))mapped[0]=1;
    for(unsigned x=1;x<512;++x){core[x]=core[x&(x-1)]|contribution[__builtin_ctz(x)];mapped[x/64]|=bit(original,core[x])<<(x%64);}
    return mapped;
}
struct Enumeration {U64 evaluated=0,zero_count=0,minimum=513,maximum=0;std::map<U64,U64>histogram;Perm best;};
Enumeration enumerate9(const Net& net,U64 limit=362880,bool progress=false) {
    if(net.n!=9)throw std::runtime_error("enumerate9 requires nine inputs");
    auto orbits=cube_orbits9(d4(9));Bits original=truth(net,identity(9));Perm p=identity(9);Enumeration result;
    do {U64 count=orbit_defect9(permute_truth9(original,p),orbits);++result.evaluated;++result.histogram[count];
        if(count==0)++result.zero_count;if(count<result.minimum){result.minimum=count;result.best=p;}result.maximum=std::max(result.maximum,count);
        if(progress && result.evaluated%32768==0) {
            std::cerr<<"{\"event\":\"enumeration_progress\",\"complete\":false,\"mappings_evaluated\":"<<result.evaluated
                <<",\"zero_count\":"<<result.zero_count<<",\"minimum_so_far\":"<<result.minimum<<",\"maximum_so_far\":"<<result.maximum<<"}\n";
        }
        if(result.evaluated==limit)break;
    }while(std::next_permutation(p.begin(),p.end()));return result;
}
std::string quote(const std::string& s) {
    std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<char(c);else if(c<32)o<<"?";else o<<char(c);}o<<'"';return o.str();
}
template<typename T>void array_json(std::ostream& o,const T& v){o<<'[';bool first=true;for(auto x:v){if(!first)o<<',';first=false;o<<x;}o<<']';}
void defect_json(std::ostream& o,const Defect& d) {
    o<<"{\"union_defect_count\":"<<d.union_count<<",\"per_transform_counts\":{";
    for(int k=0;k<8;++k){if(k)o<<',';o<<quote(names[k])<<':'<<d.each[k];}o<<"}}";
}
void write_truth(const Bits& bits,const std::string& path) {
    std::ofstream f(path,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("cannot create truth table");
    // Portable byte format: Boolean input x occupies bit x%8 of byte x/8.
    for(U64 word:bits)for(int b=0;b<8;++b)f.put(static_cast<char>((word>>(8*b))&255));
    f.close();if(!f)throw std::runtime_error("failed writing truth table");
}
std::string network_name(const std::string& path){auto s=path.substr(path.find_last_of("/\\")+1);if(s.size()>4&&s.substr(s.size()-4)==".cha")s.resize(s.size()-4);return s;}
int command(int argc,char** argv) {
    if(argc<3)throw std::runtime_error("usage: rebuild_engine eval NET p [truth.bin] | rank9 NET [p] | enumerate9 NET | inspect NET");
    const std::string mode=argv[1],path=argv[2];Net net=load_net(path);auto group=d4(net.n);
    if(mode=="inspect") {
        if(argc!=3)throw std::runtime_error("inspect argument count");
        std::cout<<"{\"network\":"<<quote(network_name(path))<<",\"n\":"<<net.n<<",\"comparators\":"<<net.comparators.size()<<",\"active_nodes\":"<<net.ops.size()<<",\"active_comparators\":"<<net.active_comparators<<",\"active_inputs\":";array_json(std::cout,net.active_inputs);std::cout<<"}\n";
    } else if(mode=="eval") {
        if(argc!=4&&argc!=5)throw std::runtime_error("eval argument count");Perm p=parse_perm(argv[3],net.n);auto id=identity(net.n);
        Bits original=truth(net,id);auto co=cofactors(original,net.n);auto identity_defect=defect(net,id,original,group);
        auto mapped_defect=(p==id?identity_defect:defect(net,p,truth(net,p),group));
        if(argc==5)write_truth(original,argv[4]);
        std::cout<<"{\"schema\":\"r4-eval-v1\",\"network\":"<<quote(network_name(path))<<",\"n\":"<<net.n<<",\"N\":"<<(U64{1}<<net.n)<<",\"mapping\":";array_json(std::cout,p);
        std::cout<<",\"mapping_direction\":\"core_pin_to_spatial\",\"ones\":"<<co.ones<<",\"A0\":";array_json(std::cout,co.A0);std::cout<<",\"A1\":";array_json(std::cout,co.A1);
        std::cout<<",\"identity\":";defect_json(std::cout,identity_defect);std::cout<<",\"mapped\":";defect_json(std::cout,mapped_defect);
        std::cout<<",\"union_defect_count\":"<<mapped_defect.union_count<<",\"active_nodes\":"<<net.ops.size()<<",\"active_comparators\":"<<net.active_comparators<<",\"total_comparators\":"<<net.comparators.size()<<",\"active_inputs\":";array_json(std::cout,net.active_inputs);
        std::cout<<",\"truth_format\":\"lsb-first bytes, input x has pin i=(x>>i)&1\",\"rank_histogram\":null}\n";
    } else if(mode=="rank9") {
        if(argc!=3&&argc!=4)throw std::runtime_error("rank9 argument count");Perm p=(argc==4?parse_perm(argv[3],net.n):identity(net.n));auto histogram=rank9(net,p);
        std::cout<<"{\"schema\":\"r4-rank-v1\",\"network\":"<<quote(network_name(path))<<",\"n\":9,\"mapping\":";array_json(std::cout,p);std::cout<<",\"permutations\":362880,\"ranks\":[1,2,3,4,5,6,7,8,9],\"histogram\":";array_json(std::cout,histogram);std::cout<<"}\n";
    } else if(mode=="enumerate9") {
        if(argc!=3)throw std::runtime_error("enumerate9 argument count");auto r=enumerate9(net,362880,true);
        if(r.evaluated!=362880)throw std::runtime_error("incomplete enumeration");
        std::cout<<"{\"schema\":\"r4-enumeration-v1\",\"network\":"<<quote(network_name(path))<<",\"n\":9,\"N\":512,\"mappings_evaluated\":"<<r.evaluated<<",\"complete\":true,\"zero_count\":"<<r.zero_count<<",\"minimum\":"<<r.minimum<<",\"maximum\":"<<r.maximum<<",\"histogram\":{";
        bool first=true;for(auto item:r.histogram){if(!first)std::cout<<',';first=false;std::cout<<quote(std::to_string(item.first))<<':'<<item.second;}std::cout<<"},\"best_witness\":";array_json(std::cout,r.best);std::cout<<"}\n";
    }else throw std::runtime_error("unknown command");
    return 0;
}
} // namespace r4
#ifndef R4_ENGINE_LIBRARY
int main(int argc,char** argv){try{return r4::command(argc,argv);}catch(const std::exception& e){std::cerr<<"rebuild_engine: "<<e.what()<<'\n';return 2;}}
#endif
