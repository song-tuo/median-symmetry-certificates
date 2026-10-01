// Fresh implementation for the one-shot mom25 structural gate.
// No historical scientific evaluator is included or read by this program.
// Truth representation: f(x) is byte[x/8] bit[x%8]; input i is (x>>i)&1.
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using U32 = std::uint32_t;
using U64 = std::uint64_t;

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
static void require(bool ok, const std::string& why) { if (!ok) throw Error(why); }
static std::string quote(const std::string& s) {
    std::ostringstream o; o << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') o << '\\' << c;
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if (c < 32) o << '?';
        else o << c;
    }
    o << '"'; return o.str();
}
template<class T> static std::string array_json(const std::vector<T>& xs) {
    std::ostringstream o; o << '[';
    for (std::size_t i=0; i<xs.size(); ++i) { if (i) o << ','; o << xs[i]; }
    o << ']'; return o.str();
}
static std::string blocks_json(const std::vector<std::vector<unsigned>>& bs) {
    std::ostringstream o; o << '[';
    for (std::size_t i=0; i<bs.size(); ++i) { if (i) o << ','; o << array_json(bs[i]); }
    o << ']'; return o.str();
}
static const char* boolean(bool b) { return b ? "true" : "false"; }
static unsigned pop(U32 x) { return static_cast<unsigned>(__builtin_popcount(x)); }
static unsigned pop64(U64 x) { return static_cast<unsigned>(__builtin_popcountll(x)); }
static std::string read_text(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    require(bool(f), "cannot open text input");
    f.seekg(0, std::ios::end); auto len=f.tellg();
    require(len>=0 && len<=1048576, "text input exceeds one MiB or size unavailable");
    f.seekg(0); std::string s(static_cast<std::size_t>(len), '\0');
    if (!s.empty()) f.read(&s[0], static_cast<std::streamsize>(s.size()));
    require(bool(f), "text read failed"); return s;
}
static void write_new(const fs::path& p, const std::string& content) {
    require(!fs::exists(p), "refusing to overwrite output: "+p.filename().string());
    std::ofstream f(p, std::ios::binary);
    require(bool(f), "cannot open output: "+p.filename().string());
    f << content;
    f.close(); require(bool(f), "output write failed: "+p.filename().string());
}
struct Gate { unsigned lo, hi, a, b, direction; };
struct Network {
    unsigned n=0, output=0;
    std::vector<Gate> gates;
    unsigned wires() const { return n+2*static_cast<unsigned>(gates.size()); }
};
// Strict recursive grammar reader, distinct from any former implementation.
// Supported header is {n,1,m,1,2,2,0}. Final field is metadata accepted only as 0.
// Every comparator must declare its consecutive output pair explicitly.
// Scientific identity mode additionally requires n=25.
class ChaReader {
    const std::string& s;
    std::size_t at=0;
    void space() { while (at<s.size() && std::isspace(static_cast<unsigned char>(s[at]))) ++at; }
    void token(char c) {
        space();
        require(at<s.size() && s[at]==c, "CHA expected delimiter at byte "+std::to_string(at));
        ++at;
    }
    unsigned number() {
        space(); require(at<s.size() && std::isdigit(static_cast<unsigned char>(s[at])), "CHA expected unsigned integer");
        U64 v=0;
        do { v=v*10+static_cast<unsigned>(s[at++]-'0'); require(v<=1000000, "CHA integer out of supported range"); }
        while (at<s.size() && std::isdigit(static_cast<unsigned char>(s[at])));
        return static_cast<unsigned>(v);
    }
public:
    explicit ChaReader(const std::string& text) : s(text) {}
    Network parse() {
        token('{'); std::array<unsigned,7> h{};
        for (unsigned i=0;i<h.size();++i) { if (i) token(','); h[i]=number(); }
        token('}');
        require(h[0]>=1 && h[0]<=25, "CHA input count outside 1..25");
        require(h[1]==1 && h[3]==1 && h[4]==2 && h[5]==2 && h[6]==0, "unsupported CHA header");
        require(h[2]<=10000, "too many CHA comparators");
        Network net; net.n=h[0];
        for (unsigned k=0;k<h[2];++k) {
            Gate g{}; token('('); token('['); g.lo=number(); token(','); g.hi=number(); token(']');
            g.a=number(); token(','); g.b=number(); token(','); g.direction=number(); token(')');
            require(g.lo==net.n+2*k && g.hi==g.lo+1, "nonconsecutive CHA output labels");
            require(g.a<g.lo && g.b<g.lo, "CHA forward or out-of-range reference");
            require(g.direction==1 || g.direction==2, "unsupported CHA min/max direction");
            net.gates.push_back(g);
        }
        token('('); net.output=number(); token(')'); space();
        require(at==s.size(), "trailing CHA material");
        require(net.output<net.wires(), "CHA final output outside wire set");
        return net;
    }
};
static Network parse_network(const fs::path& path) { std::string s=read_text(path); return ChaReader(s).parse(); }
static std::vector<std::uint8_t> read_truth(const fs::path& path, unsigned n) {
    require(n>=1 && n<=25, "truth input count outside 1..25");
    std::ifstream f(path, std::ios::binary);
    require(bool(f), "cannot open saved truth");
    std::size_t bytes=((std::size_t(1)<<n)+7)/8;
    f.seekg(0,std::ios::end);
    require(f.tellg()==static_cast<std::streamoff>(bytes), "saved truth byte length mismatch");
    f.seekg(0); std::vector<std::uint8_t> data(bytes);
    f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    require(bool(f), "saved truth read failed");
    if (n<3) require((data[0] >> (1u<<n))==0, "nonzero padding bits in small truth");
    return data;
}
static bool bit_at(const std::vector<std::uint8_t>& data, U32 x) { return ((data[x/8]>>(x%8))&1u)!=0; }
static std::vector<std::uint8_t> pack_bits(const std::vector<U32>& bits) {
    std::vector<std::uint8_t> data((bits.size()+7)/8, 0);
    for (std::size_t x=0;x<bits.size();++x) {
        require(bits[x]<=1, "not a Boolean fixture");
        data[x/8] |= static_cast<std::uint8_t>(bits[x]<<(x%8));
    }
    return data;
}
static U64 primary_word(unsigned pin, U32 base, unsigned valid=64) {
    // Directly construct low-frequency patterns once per pin for each word.
    // For pin >=6, an aligned 64-input chunk is constant in that coordinate.
    if (pin>=6) return ((base>>pin)&1u) ? ~U64(0) : U64(0);
    U64 word=0;
    for (unsigned b=0;b<valid;++b) if (((base+b)>>pin)&1u) word |= U64(1)<<b;
    return word;
}
static std::array<U64,6> small_patterns() {
    std::array<U64,6> a{};
    for (unsigned i=0;i<6;++i) a[i]=primary_word(i,0);
    return a;
}
static U64 evaluate_word(const Network& net, U32 base, std::vector<U64>& wires,
                         const std::array<U64,6>& patterns) {
    for (unsigned i=0;i<net.n;++i)
        wires[i]=i<6 ? patterns[i] : (((base>>i)&1u) ? ~U64(0) : U64(0));
    for (const auto& g:net.gates) {
        U64 low=wires[g.a]&wires[g.b], high=wires[g.a]|wires[g.b];
        wires[g.lo]=g.direction==1 ? low : high;
        wires[g.hi]=g.direction==1 ? high : low;
    }
    return wires[net.output];
}
static bool evaluate_scalar(const Network& net, U32 x, unsigned output=std::numeric_limits<unsigned>::max()) {
    std::vector<unsigned> w(net.wires());
    for (unsigned i=0;i<net.n;++i) w[i]=(x>>i)&1u;
    for (const auto& g:net.gates) {
        unsigned a=w[g.a], b=w[g.b];
        w[g.lo]=g.direction==1 ? std::min(a,b) : std::max(a,b);
        w[g.hi]=g.direction==1 ? std::max(a,b) : std::min(a,b);
    }
    return w[output==std::numeric_limits<unsigned>::max() ? net.output : output]!=0;
}
// Majority uses the 10 three-literal implicants, with no comparator evaluation.
static U64 majority5(const std::array<U64,5>& x) {
    U64 result=0;
    for (unsigned a=0;a<5;++a) for (unsigned b=a+1;b<5;++b) for (unsigned c=b+1;c<5;++c)
        result |= x[a]&x[b]&x[c];
    return result;
}
static U64 mom_word(U32 base, const std::array<U64,6>& patterns) {
    std::array<U64,5> outer{};
    for (unsigned g=0;g<5;++g) {
        std::array<U64,5> inner{};
        for (unsigned t=0;t<5;++t) {
            unsigned pin=5*g+t;
            inner[t]=pin<6 ? patterns[pin] : (((base>>pin)&1u) ? ~U64(0) : U64(0));
        }
        outer[g]=majority5(inner);
    }
    return majority5(outer);
}
static U64 saved_word(const std::vector<std::uint8_t>& data, U32 base) {
    U64 word=0;
    for (unsigned j=0;j<8;++j) word |= U64(data[base/8+j])<<(8*j);
    return word;
}
struct Difference {
    U64 count=0;
    U64 first=std::numeric_limits<U64>::max();
    void add(U64 word,U32 base) {
        count+=pop64(word);
        if (word && first==std::numeric_limits<U64>::max()) first=U64(base)+static_cast<unsigned>(__builtin_ctzll(word));
    }
    std::string json() const {
        return "{\"count\":"+std::to_string(count)+",\"first_input\":"+
            (first==std::numeric_limits<U64>::max() ? "null" : std::to_string(first))+"}";
    }
};
static int identity(const fs::path& netpath,const fs::path& truthpath,const fs::path& output) {
    require(!fs::exists(output), "identity output already exists");
    Network net=parse_network(netpath);
    require(net.n==25, "formal identity requires 25 inputs");
    auto data=read_truth(truthpath,25);
    auto patterns=small_patterns();
    std::vector<U64> wires(net.wires());
    Difference ns, ms, nm; U64 netones=0, momones=0, savedones=0;
    constexpr U32 N=U32(1)<<25;
    for (U32 base=0;base<N;base+=64) {
        U64 a=evaluate_word(net,base,wires,patterns), b=mom_word(base,patterns), c=saved_word(data,base);
        ns.add(a^c,base); ms.add(b^c,base); nm.add(a^b,base);
        netones+=pop64(a); momones+=pop64(b); savedones+=pop64(c);
    }
    bool ok=ns.count==0 && ms.count==0 && nm.count==0;
    std::ostringstream o;
    o << "{\n\"status\":" << quote(ok?"PASS":"FUNCTION_IDENTITY_FAILED")
      << ",\n\"n\":25,\"N\":" << N << ",\"input_bits_compared\":" << N
      << ",\n\"truth_encoding\":\"byte[x/8] bit[x%8]; pin_i=(x>>i)&1\""
      << ",\n\"network\":" << quote(netpath.filename().string())
      << ",\"comparators\":" << net.gates.size()
      << ",\n\"independent_formula\":\"Maj5(Maj5(x0..x4),Maj5(x5..x9),Maj5(x10..x14),Maj5(x15..x19),Maj5(x20..x24))\""
      << ",\n\"netlist_vs_saved\":" << ns.json()
      << ",\n\"formula_vs_saved\":" << ms.json()
      << ",\n\"netlist_vs_formula\":" << nm.json()
      << ",\n\"ones\":{\"netlist\":" << netones << ",\"formula\":" << momones << ",\"saved\":" << savedones << "}\n}\n";
    write_new(output,o.str()); std::cout<<o.str();
    return ok ? 0 : 20;
}
// Upper-subset zeta: z[S]=sum_{x: (x&S)==S} f(x).
// Values cannot exceed 2^25; uint32_t is exact at all intermediate steps.
static std::vector<U32> upper_zeta(const std::vector<std::uint8_t>& truth,unsigned n) {
    const U32 size=U32(1)<<n;
    std::vector<U32> z(size);
    for (U32 x=0;x<size;++x) z[x]=bit_at(truth,x);
    for (unsigned i=0;i<n;++i) {
        U32 step=U32(1)<<i;
        for (U32 start=0;start<size;start+=2*step)
            for (U32 j=0;j<step;++j) z[start+j]+=z[start+j+step];
    }
    return z;
}
static U32 cofactor_ones(const std::vector<U32>& z,const std::vector<unsigned>& pins,unsigned pattern) {
    U32 ones=0,zeros=0;
    require(pins.size()<=25 && pattern<(U32(1)<<pins.size()),"invalid cofactor pattern");
    for (unsigned j=0;j<pins.size();++j) {
        U32 bit=U32(1)<<pins[j];
        require((ones&bit)==0 && (zeros&bit)==0, "repeated cofactor pin");
        ((pattern>>(pins.size()-1-j))&1u ? ones : zeros) |= bit;
    }
    std::int64_t total=0;
    U32 sub=zeros;
    for (;;) {
        total += (pop(sub)&1u) ? -std::int64_t(z[ones|sub]) : std::int64_t(z[ones|sub]);
        if (sub==0) break;
        sub=(sub-1)&zeros;
    }
    require(total>=0 && total<=std::numeric_limits<U32>::max(),"invalid cofactor inclusion-exclusion result");
    return static_cast<U32>(total);
}
static U32 brute_cofactor(const std::vector<U32>& bits,const std::vector<unsigned>& pins,unsigned pattern) {
    U32 count=0;
    for (U32 x=0;x<bits.size();++x) {
        bool good=true;
        for (unsigned j=0;j<pins.size();++j)
            good &= ((x>>pins[j])&1u)==((pattern>>(pins.size()-1-j))&1u);
        if (good) count+=bits[x];
    }
    return count;
}
struct Triple { unsigned a,b,c; U32 count; };
struct Recovery {
    U32 minimum=0;
    std::vector<std::vector<unsigned>> blocks;
    std::vector<unsigned> membership;
    std::vector<std::array<unsigned,2>> edges;
    std::vector<std::string> failures;
    std::map<unsigned,std::map<U32,unsigned>> by_type;
    bool graph_ok=false, relationships_ok=false;
};
// This function takes only measured triples and number of pins. It receives no
// netlist, group labels, candidate partition, expected count or integer/5 labels.
static Recovery recover_blind(unsigned n,const std::vector<Triple>& ts) {
    require(!ts.empty(),"empty triple collection");
    Recovery r; r.minimum=ts.front().count;
    for (const auto& t:ts) r.minimum=std::min(r.minimum,t.count);
    std::vector<std::vector<bool>> adjacent(n,std::vector<bool>(n,false));
    for (const auto& t:ts) if (t.count==r.minimum) {
        for (auto pair: {std::array<unsigned,2>{t.a,t.b}, {t.a,t.c}, {t.b,t.c}})
            adjacent[pair[0]][pair[1]]=adjacent[pair[1]][pair[0]]=true;
    }
    r.membership.assign(n,n);
    for (unsigned start=0;start<n;++start) if (r.membership[start]==n) {
        unsigned label=static_cast<unsigned>(r.blocks.size()); r.blocks.push_back({});
        std::queue<unsigned> todo; todo.push(start); r.membership[start]=label;
        while (!todo.empty()) {
            unsigned i=todo.front(); todo.pop(); r.blocks.back().push_back(i);
            for (unsigned j=0;j<n;++j) if (adjacent[i][j] && r.membership[j]==n) {
                r.membership[j]=label; todo.push(j);
            }
        }
        std::sort(r.blocks.back().begin(),r.blocks.back().end());
    }
    for (unsigned i=0;i<n;++i) for (unsigned j=i+1;j<n;++j)
        if (adjacent[i][j]) r.edges.push_back({i,j});
    if (r.blocks.size()!=5) r.failures.push_back("component_count_is_not_five");
    for (const auto& b:r.blocks) {
        if (b.size()!=5) r.failures.push_back("component_size_is_not_five");
        for (unsigned i:b) for (unsigned j:b) if (i!=j && !adjacent[i][j])
            r.failures.push_back("component_is_not_clique");
    }
    for (const auto& t:ts) {
        bool same=r.membership[t.a]==r.membership[t.b] && r.membership[t.b]==r.membership[t.c];
        if ((t.count==r.minimum)!=same) r.failures.push_back("minimum_relation_is_not_complete_within_blocks");
    }
    r.graph_ok=r.failures.empty();
    // Deliberately postpone triple-type association until graph recovery passes.
    if (r.graph_ok) {
        for (const auto& t:ts) {
            std::set<unsigned> distinct={r.membership[t.a],r.membership[t.b],r.membership[t.c]};
            r.by_type[static_cast<unsigned>(distinct.size())][t.count]++;
        }
        r.relationships_ok=r.by_type.size()==3;
        for (const auto& kv:r.by_type) r.relationships_ok &= kv.second.size()==1;
        std::set<U32> values;
        for (const auto& kv:r.by_type) for (const auto& cv:kv.second) values.insert(cv.first);
        r.relationships_ok &= values.size()==3;
        if (!r.relationships_ok) r.failures.push_back("types_do_not_have_three_distinct_uniform_counts");
    }
    std::sort(r.failures.begin(),r.failures.end());
    r.failures.erase(std::unique(r.failures.begin(),r.failures.end()),r.failures.end());
    return r;
}
static std::string string_array(const std::vector<std::string>& ss) {
    std::ostringstream o; o<<'[';
    for (std::size_t i=0;i<ss.size();++i) {if(i)o<<',';o<<quote(ss[i]);}
    o<<']'; return o.str();
}
static std::string cofactor_table_json(const std::vector<U32>& z,const std::vector<unsigned>& pins,unsigned n) {
    std::ostringstream o; o<<"{\"pins\":"<<array_json(pins)<<",\"cells\":[";
    U32 capacity=U32(1)<<(n-pins.size());
    for (unsigned pattern=0;pattern<(1u<<pins.size());++pattern) {
        if(pattern)o<<',';
        std::vector<unsigned> assignment;
        for(unsigned j=0;j<pins.size();++j)assignment.push_back((pattern>>(pins.size()-1-j))&1u);
        U32 ones=cofactor_ones(z,pins,pattern); require(ones<=capacity,"cofactor exceeds capacity");
        o<<"{\"assignment\":"<<array_json(assignment)<<",\"zeros\":"<<capacity-ones<<",\"ones\":"<<ones<<'}';
    }
    o<<"]}";return o.str();
}
static std::vector<U32> signature(const std::vector<U32>& z,const std::vector<unsigned>& pins,unsigned n) {
    std::vector<U32> v;
    U32 capacity=U32(1)<<(n-pins.size());
    for(unsigned pattern=0;pattern<(1u<<pins.size());++pattern) {
        U32 a=cofactor_ones(z,pins,pattern);
        require(a<=capacity,"cofactor exceeds capacity");
        v.push_back(capacity-a);v.push_back(a);
    }
    return v;
}
static int structure(const fs::path& truthpath,const fs::path& outdir) {
    const std::vector<std::string> names={"first_order.json","first_order.csv","second_order.json","second_order.csv",
        "all_first_second_order_signatures.json","third_order.json","third_order.csv",
        "all_2300_third_order_counts.csv","recovered_blocks.json","recovered_blocks.csv","triple_types_post_recovery.csv"};
    require(fs::is_directory(outdir),"structure output directory must already exist");
    for(const auto& f:names)require(!fs::exists(outdir/f),"structure output already exists: "+f);
    constexpr unsigned n=25;
    auto truth=read_truth(truthpath,n);
    auto z=upper_zeta(truth,n);
    std::ostringstream first, second, fc, sc;
    first<<"{\"n\":25,\"source\":\"saved_truth_only\",\"W\":"<<z[0]<<",\"tables\":[";
    second<<"{\"n\":25,\"source\":\"saved_truth_only\",\"W\":"<<z[0]<<",\"tables\":[";
    fc<<"pin,assignment,zeros,ones\n";
    sc<<"pin_i,pin_j,assignment_i,assignment_j,zeros,ones\n";
    auto s1=signature(z,{0},n),s2=signature(z,{0,1},n);
    bool eq1=true,eq2=true;
    unsigned pairs=0;
    for(unsigned i=0;i<n;++i) {
        if(i)first<<',';
        first<<cofactor_table_json(z,{i},n);
        auto sig=signature(z,{i},n);eq1 &= sig==s1;
        for(unsigned a=0;a<2;++a)fc<<i<<','<<a<<','<<sig[2*a]<<','<<sig[2*a+1]<<'\n';
        for(unsigned j=i+1;j<n;++j) {
            if(pairs++)second<<',';
            second<<cofactor_table_json(z,{i,j},n);
            auto pair=signature(z,{i,j},n);eq2 &= pair==s2;
            for(unsigned p=0;p<4;++p)sc<<i<<','<<j<<','<<(p>>1)<<','<<(p&1)<<','<<pair[2*p]<<','<<pair[2*p+1]<<'\n';
        }
    }
    first<<"],\"table_count\":25,\"all_complete_signatures_equal\":"<<boolean(eq1)
         <<",\"signature_cell_order\":\"assignment lexicographic; zeros,ones\",\"reference_signature\":"<<array_json(s1)<<"}\n";
    second<<"],\"table_count\":"<<pairs<<",\"all_complete_signatures_equal\":"<<boolean(eq2)
          <<",\"signature_cell_order\":\"assignment lexicographic; zeros,ones\",\"reference_signature\":"<<array_json(s2)<<"}\n";
    write_new(outdir/"first_order.json",first.str());write_new(outdir/"first_order.csv",fc.str());
    write_new(outdir/"second_order.json",second.str());write_new(outdir/"second_order.csv",sc.str());
    write_new(outdir/"all_first_second_order_signatures.json",
        "{\"source\":\"saved_truth_only\",\"first_order\":"+first.str()+",\"second_order\":"+second.str()+"}\n");
    if(!eq1 || !eq2) {
        std::cout<<"{\"status\":\"IMPLEMENTATION_INVALID\",\"reason\":\"LOWER_ORDER_SIGNATURE_MISMATCH\",\"first_order_equal\":"
                 <<boolean(eq1)<<",\"second_order_equal\":"<<boolean(eq2)<<"}\n"; return 21;
    }
    std::vector<Triple> triples;
    std::map<U32,unsigned> histogram;
    std::ostringstream tj,tc;
    tj<<"{\"n\":25,\"source\":\"saved_truth_only\",\"assignment\":[1,1,1],\"triples\":[";
    tc<<"pin_i,pin_j,pin_k,ones\n";
    for(unsigned i=0;i<n;++i)for(unsigned j=i+1;j<n;++j)for(unsigned k=j+1;k<n;++k) {
        U32 count=z[(U32(1)<<i)|(U32(1)<<j)|(U32(1)<<k)];
        if(!triples.empty())tj<<',';
        triples.push_back({i,j,k,count});histogram[count]++;
        tj<<"{\"pins\":["<<i<<','<<j<<','<<k<<"],\"ones\":"<<count<<'}';
        tc<<i<<','<<j<<','<<k<<','<<count<<'\n';
    }
    tj<<"],\"triple_count\":"<<triples.size()<<",\"histogram\":[";
    bool comma=false;
    for(const auto& h:histogram){if(comma)tj<<',';comma=true;tj<<"{\"ones\":"<<h.first<<",\"triples\":"<<h.second<<'}';}
    tj<<"]}\n";
    write_new(outdir/"third_order.json",tj.str());write_new(outdir/"third_order.csv",tc.str());
    write_new(outdir/"all_2300_third_order_counts.csv",tc.str());
    Recovery r=recover_blind(n,triples);
    // User-specified theoretical targets are assertions only. They have not
    // entered the zeta transform, table generation, minimal relation or graph.
    const std::map<unsigned,U32> expected={{1,2883584},{2,2981888},{3,2968064}};
    bool expected_ok=r.relationships_ok;
    if(r.relationships_ok)for(const auto& e:expected)
        expected_ok &= r.by_type.at(e.first).begin()->first==e.second;
    if(!expected_ok)r.failures.push_back("post_recovery_expected_count_assertion_failed");
    bool ok=r.graph_ok && r.relationships_ok && expected_ok && triples.size()==2300;
    std::ostringstream bj,bc,types;
    bj<<"{\"status\":"<<quote(ok?"PASS":"THIRD_ORDER_BLOCK_RECOVERY_FAILED")
      <<",\"recovery_inputs\":\"truth-derived triples only; no netlist or partition labels\""
      <<",\"minimum_observed_count\":"<<r.minimum<<",\"blocks\":"<<blocks_json(r.blocks)
      <<",\"component_count\":"<<r.blocks.size()<<",\"edges\":[";
    for(std::size_t q=0;q<r.edges.size();++q){if(q)bj<<',';bj<<'['<<r.edges[q][0]<<','<<r.edges[q][1]<<']';}
    bj<<"],\"graph_is_five_disjoint_K5\":"<<boolean(r.graph_ok)
      <<",\"type_relationships_verified_after_recovery\":"<<boolean(r.relationships_ok)
      <<",\"user_target_assertions_passed\":"<<boolean(expected_ok)<<",\"post_recovery_types\":[";
    comma=false;
    for(const auto& kv:r.by_type)for(const auto& cv:kv.second) {
        if(comma)bj<<',';comma=true;
        bj<<"{\"distinct_blocks\":"<<kv.first<<",\"ones\":"<<cv.first<<",\"triples\":"<<cv.second<<'}';
    }
    bj<<"],\"failures\":"<<string_array(r.failures)<<"}\n";
    bc<<"block,pin\n";
    for(unsigned b=0;b<r.blocks.size();++b)for(unsigned pin:r.blocks[b])bc<<b<<','<<pin<<'\n';
    types<<"pin_i,pin_j,pin_k,distinct_recovered_blocks,ones\n";
    if(r.graph_ok)for(const auto& t:triples) {
        std::set<unsigned> ds={r.membership[t.a],r.membership[t.b],r.membership[t.c]};
        types<<t.a<<','<<t.b<<','<<t.c<<','<<ds.size()<<','<<t.count<<'\n';
    }
    write_new(outdir/"recovered_blocks.json",bj.str());write_new(outdir/"recovered_blocks.csv",bc.str());
    write_new(outdir/"triple_types_post_recovery.csv",types.str());
    std::cout<<bj.str();
    return ok?0:22;
}
static std::vector<std::vector<unsigned>> read_blocks_csv(const fs::path& path) {
    std::istringstream in(read_text(path));std::string line;
    require(bool(std::getline(in,line)) && line=="block,pin","invalid recovered block CSV header");
    std::map<unsigned,std::vector<unsigned>> mapping;std::set<unsigned> seen;
    while(std::getline(in,line)) {
        require(!line.empty(),"blank recovered block row");
        auto comma=line.find(',');require(comma!=std::string::npos && line.find(',',comma+1)==std::string::npos,"invalid recovered block row");
        auto decimal=[](const std::string& x) {
            require(!x.empty(),"empty recovered block number");unsigned n=0;
            for(char c:x){require(c>='0'&&c<='9',"nondecimal recovered block number");n=n*10+static_cast<unsigned>(c-'0');require(n<25,"recovered block number out of range");}
            return n;
        };
        unsigned b=decimal(line.substr(0,comma)),pin=decimal(line.substr(comma+1));
        require(seen.insert(pin).second,"repeated recovered block pin");mapping[b].push_back(pin);
    }
    require(mapping.size()==5 && seen.size()==25,"recovered CSV not a five-by-five partition");
    std::vector<std::vector<unsigned>> blocks;
    for(auto& kv:mapping){require(kv.second.size()==5,"recovered block not size five");std::sort(kv.second.begin(),kv.second.end());blocks.push_back(kv.second);}
    std::sort(blocks.begin(),blocks.end());return blocks;
}
static int posthoc(const fs::path& netpath,const fs::path& blockspath,const fs::path& output) {
    require(!fs::exists(output),"posthoc output already exists");
    // Parse and extract from the netlist before loading recovered labels.
    Network net=parse_network(netpath);require(net.n==25,"posthoc requires 25-input network");
    std::vector<U32> support(net.wires());
    for(unsigned i=0;i<net.n;++i)support[i]=U32(1)<<i;
    for(const auto& g:net.gates)support[g.lo]=support[g.hi]=support[g.a]|support[g.b];
    std::vector<bool> active(net.wires(),false);active[net.output]=true;
    for(std::size_t k=net.gates.size();k>0;--k) {
        const auto& g=net.gates[k-1];
        if(active[g.lo]||active[g.hi])active[g.a]=active[g.b]=true;
    }
    std::set<U32> module_supports;
    std::map<U32,std::set<unsigned>> outgoing;
    for(unsigned w=0;w<net.wires();++w)if(active[w]&&pop(support[w])==5)module_supports.insert(support[w]);
    for(const auto& g:net.gates)if(active[g.lo]||active[g.hi])
        for(unsigned input:{g.a,g.b})if(pop(support[input])==5 && pop(support[g.lo])>5)outgoing[support[input]].insert(input);
    std::vector<std::vector<unsigned>> extracted;
    std::ostringstream details;details<<'[';bool comma=false,semantics_ok=true;
    U32 covered=0;bool disjoint=true;
    for(U32 mask:module_supports) {
        std::vector<unsigned> pins;
        for(unsigned i=0;i<net.n;++i)if((mask>>i)&1u)pins.push_back(i);
        extracted.push_back(pins);disjoint &= (covered&mask)==0;covered|=mask;
        const auto& boundaries=outgoing[mask];bool local_ok=boundaries.size()==1;
        unsigned witness=boundaries.empty()?net.wires():*boundaries.begin();
        if(local_ok)for(U32 local=0;local<32;++local) {
            U32 global=0;for(unsigned j=0;j<5;++j)if((local>>j)&1u)global|=U32(1)<<pins[j];
            local_ok &= evaluate_scalar(net,global,witness)==(pop(local)>=3);
        }
        semantics_ok &= local_ok;
        if(comma)details<<',';comma=true;
        details<<"{\"syntactic_support\":"<<array_json(pins)<<",\"outgoing_wires\":[";
        bool c=false;for(unsigned wire:boundaries){if(c)details<<',';c=true;details<<wire;}
        details<<"],\"unique_boundary_is_majority5_on_all_32_assignments\":"<<boolean(local_ok)<<'}';
    }
    details<<']';std::sort(extracted.begin(),extracted.end());
    auto recovered=read_blocks_csv(blockspath);
    bool match=extracted==recovered;
    bool ok=module_supports.size()==5 && disjoint && covered==((U32(1)<<25)-1) && semantics_ok && match;
    std::ostringstream o;
    o<<"{\"status\":"<<quote(ok?"PASS":"IMPLEMENTATION_INVALID")
     <<",\"stage\":\"posthoc_netlist_support_comparison\",\"extracted_before_loading_recovered_labels\":true"
     <<",\"extracted_blocks\":"<<blocks_json(extracted)<<",\"recovered_blocks\":"<<blocks_json(recovered)
     <<",\"matching_partitions\":"<<boolean(match)<<",\"disjoint_full_cover\":"<<boolean(disjoint&&covered==((U32(1)<<25)-1))
     <<",\"module_details\":"<<details.str()<<"}\n";
    write_new(output,o.str());std::cout<<o.str();return ok?0:23;
}
static int self_test() {
    U64 checks=0;
    auto check=[&](bool ok,const std::string& why){++checks;require(ok,"self-test: "+why);};
    auto parse=[](const std::string& s){return ChaReader(s).parse();};
    auto rejected=[&](const std::string& s){bool threw=false;try{(void)parse(s);}catch(const Error&){threw=true;}check(threw,"parser accepted malformed input");};
    auto patterns=small_patterns();
    // Two output directions and both final wires, over all four assignments.
    for(unsigned direction=1;direction<=2;++direction)for(unsigned output=2;output<=3;++output) {
        std::string s="{2,1,1,1,2,2,0}([2,3]0,1,"+std::to_string(direction)+")("+std::to_string(output)+")";
        auto net=parse(s);std::vector<U64>w(net.wires());U64 word=evaluate_word(net,0,w,patterns);
        for(U32 x=0;x<4;++x) {
            bool want=((direction==1)==(output==2)) ? ((x&3)==3) : ((x&3)!=0);
            check(evaluate_scalar(net,x)==want,"scalar min/max direction");
            check(((word>>x)&1u)==want,"word min/max direction");
        }
    }
    check(parse(" \n{1,1,0,1,2,2,0} (0) \t").n==1,"whitespace parser");
    for(const auto& s:std::vector<std::string>{
        "", "{0,1,0,1,2,2,0}(0)", "{26,1,0,1,2,2,0}(0)",
        "{2,2,0,1,2,2,0}(0)", "{2,1,0,1,2,2,1}(0)",
        "{2,1,1,1,2,2,0}([3,4]0,1,1)(3)",
        "{2,1,1,1,2,2,0}([2,3]2,1,1)(3)",
        "{2,1,1,1,2,2,0}([2,3]0,1,3)(3)",
        "{2,1,1,1,2,2,0}([2,3]0,1,1)(4)",
        "{2,1,0,1,2,2,0}(0)trailing",
        "{2,1,0,1,2,2,0}(-1)", "{2,1,0,1,2,2,0}(1000001)",
        "{2,1,0,1,2,2,0}(0)(1)", "{2,1,1,1,2,2,0}(0)"})rejected(s);
    // A separate 5-input sorting fixture built by insertion sorting. Its
    // comparator topology is intentionally not the fixed mom netlist topology.
    std::ostringstream nettext;std::array<unsigned,5> pins={0,1,2,3,4};
    nettext<<"{5,1,10,1,2,2,0}";
    unsigned next=5;
    for(unsigned i=1;i<5;++i)for(unsigned j=i;j>0;--j) {
        nettext<<"(["<<next<<','<<next+1<<']'<<pins[j-1]<<','<<pins[j]<<",1)";
        pins[j-1]=next;pins[j]=next+1;next+=2;
    }
    nettext<<'('<<pins[2]<<')';auto median=parse(nettext.str());std::vector<U64> mw(median.wires());
    U64 medianword=evaluate_word(median,0,mw,patterns);
    std::array<U64,5> major_input={patterns[0],patterns[1],patterns[2],patterns[3],patterns[4]};
    U64 majorword=majority5(major_input);
    for(U32 x=0;x<32;++x){
        check(evaluate_scalar(median,x)==(pop(x)>=3),"sorting fixture median");
        check(((medianword>>x)&1u)==(pop(x)>=3),"word sorting fixture median");
        check(((majorword>>x)&1u)==(pop(x)>=3),"majority implicant formula");
    }
    // Explicit nonzero word bases exercise all input bits and byte orientation.
    for(U32 base:{0u,64u,128u,256u,448u})for(unsigned pin=0;pin<9;++pin)
        for(unsigned bit=0;bit<64;++bit)check(((primary_word(pin,base)>>bit)&1u)==(((base+bit)>>pin)&1u),"primary bit position");
    std::vector<U32> bitfixture(512,0);
    for(U32 x:{0u,1u,7u,8u,31u,63u,64u,127u,255u,256u,511u})bitfixture[x]=1;
    auto packed=pack_bits(bitfixture);
    for(U32 x=0;x<512;++x)check(bit_at(packed,x)==bool(bitfixture[x]),"packed byte bit order");
    for(U32 base=0;base<512;base+=64)for(unsigned b=0;b<64;++b)
        check(((saved_word(packed,base)>>b)&1u)==bitfixture[base+b],"saved word byte assembly");
    // Deterministic complete small functions; no full scientific truth is read.
    for(unsigned n=1;n<=9;++n)for(unsigned family=0;family<6;++family) {
        std::vector<U32> bits(U32(1)<<n);
        for(U32 x=0;x<bits.size();++x) {
            unsigned weight=pop(x);
            switch(family) {
                case 0: bits[x]=0;break;
                case 1: bits[x]=1;break;
                case 2: bits[x]=weight&1;break;
                case 3: bits[x]=2*weight>=n+1;break;
                case 4: bits[x]=(x&1u)&&((x>>(n-1))&1u);break;
                default:bits[x]=((x&1u)^((x>>(n-1))&1u))||((x&6u)==6u);break;
            }
        }
        auto z=upper_zeta(pack_bits(bits),n);
        for(U32 mask=0;mask<bits.size();++mask) {
            U32 direct=0;for(U32 x=0;x<bits.size();++x)if((x&mask)==mask)direct+=bits[x];
            check(z[mask]==direct,"upper zeta versus direct complete sum");
        }
        for(U32 mask=0;mask<bits.size();++mask)if(pop(mask)<=3) {
            std::vector<unsigned> selected;for(unsigned p=0;p<n;++p)if((mask>>p)&1u)selected.push_back(p);
            for(unsigned b=0;b<(1u<<selected.size());++b)
                check(cofactor_ones(z,selected,b)==brute_cofactor(bits,selected,b),"all cofactor assignments via inclusion-exclusion");
        }
    }
    // A relabeled synthetic relation proves graph extraction does not use pin/5.
    // These triples are an engineering fixture, not measured scientific values.
    std::vector<Triple> fixture;
    for(unsigned i=0;i<25;++i)for(unsigned j=i+1;j<25;++j)for(unsigned k=j+1;k<25;++k) {
        std::set<unsigned> classes={i%5,j%5,k%5};
        U32 value=classes.size()==1?7u:(classes.size()==2?13u:11u);
        fixture.push_back({i,j,k,value});
    }
    auto rec=recover_blind(25,fixture);
    check(rec.graph_ok&&rec.relationships_ok,"blind permuted synthetic relation recovery");
    for(unsigned i=0;i<25;++i)for(unsigned j=0;j<25;++j)
        check((rec.membership[i]==rec.membership[j])==(i%5==j%5),"recovered synthetic membership");
    fixture.front().count=6;
    check(!recover_blind(25,fixture).graph_ok,"damaged minimal relation rejected");
    std::cout<<"{\"status\":\"PASS\",\"mode\":\"small_deterministic_engineering_self_test\",\"assertions\":"<<checks
             <<",\"largest_truth_input_count\":9,\"full_saved_truth_read\":false,\"randomness\":false}\n";
    return 0;
}
static int dispatch(int argc,char**argv) {
    if(argc==2 && std::string(argv[1])=="--self-test")return self_test();
    if(argc==5 && std::string(argv[1])=="identity")return identity(argv[2],argv[3],argv[4]);
    if(argc==4 && std::string(argv[1])=="structure")return structure(argv[2],argv[3]);
    if(argc==5 && std::string(argv[1])=="posthoc")return posthoc(argv[2],argv[3],argv[4]);
    throw Error("usage: --self-test | identity NETLIST SAVED_TRUTH OUT_JSON | structure SAVED_TRUTH EXISTING_OUT_DIR | posthoc NETLIST RECOVERED_BLOCKS_CSV OUT_JSON");
}
#ifndef FUNCTION_STRUCTURE_NO_MAIN
int main(int argc,char**argv) {
    try{return dispatch(argc,argv);}
    catch(const std::exception& e) {
        std::cout<<"{\"status\":\"IMPLEMENTATION_INVALID\",\"error\":"<<quote(e.what())<<"}\n";
        return 2;
    }
}
#endif
