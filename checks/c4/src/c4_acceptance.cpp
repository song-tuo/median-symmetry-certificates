// Transparent fixed-wiring acceptance adapter; no optimizer or new evaluator.
#define R4_ENGINE_LIBRARY
#include "../../../src/rebuild_engine.cpp"
#include <filesystem>
namespace fs=std::filesystem;
using r4::Perm;using r4::Bits;using r4::U64;
static void ensure(bool b,const std::string&m){if(!b)throw std::runtime_error(m);}
static Perm inverse(const Perm&p){r4::check_perm(p,static_cast<int>(p.size()));Perm q(p.size());for(int i=0;i<static_cast<int>(p.size());++i)q[p[i]]=i;return q;}
static std::vector<Perm> active_group(int side){
    int h=side/2;std::vector<Perm> gs;
    // Fixed order e,r,r2,r3,s,rs,r2s,r3s; s(x,y)=(x,-y).
    for(int refl=0;refl<2;++refl)for(int k=0;k<4;++k){
        Perm g(side*side);
        for(int j=0;j<side*side;++j){
            int x=j%side-h,y=j/side-h;if(refl)y=-y;
            for(int t=0;t<k;++t){int nx=-y;y=x;x=nx;}
            g[j]=(x+h)+side*(y+h);
        }r4::check_perm(g,side*side);gs.push_back(g);
    }return gs;
}
static const std::array<const char*,8> labels={"identity","r","r2","r3","s","rs","r2s","r3s"};
static std::string array(const Perm&p){std::ostringstream o;r4::array_json(o,p);return o.str();}
static void save_new(const fs::path&p,const std::string&s){
    ensure(!fs::exists(p),"refuse overwrite: "+p.filename().string());
    std::ofstream out(p,std::ios::binary);ensure(bool(out),"output open failed");out<<s;out.close();ensure(bool(out),"output write failed");
}
static Bits saved_truth(const fs::path&p){
    std::ifstream in(p,std::ios::binary);ensure(bool(in),"saved truth open failed");
    in.seekg(0,std::ios::end);ensure(in.tellg()==4194304,"saved truth length mismatch");in.seekg(0);
    Bits bits(4194304/8);
    for(auto&w:bits){w=0;for(unsigned k=0;k<8;++k){int c=in.get();ensure(c!=EOF,"saved truth truncated");w|=U64(static_cast<unsigned char>(c))<<(8*k);}}
    ensure(in.peek()==EOF,"saved truth has extra bytes");return bits;
}
static U64 transform_input(U64 x,const Perm&g){
    Perm gi=inverse(g);U64 y=0;
    for(unsigned j=0;j<g.size();++j)y|=((x>>gi[j])&1)<<j;
    return y;
}
static int scalar_spatial(const r4::Net&net,const Perm&p,U64 x){
    std::vector<int>v(net.n);for(int j=0;j<net.n;++j)v[j]=(x>>p[j])&1;return r4::scalar(net,v);
}
struct Counts{std::array<U64,8>each{};U64 combined=0;U64 s_first=std::numeric_limits<U64>::max();int before=-1,after=-1;};
static Counts evaluate_transforms(const r4::Net&net,const Perm&p,const Bits&base,const std::vector<Perm>&gs){
    Counts out;Bits union_bits(base.size(),0);
    for(unsigned t=1;t<gs.size();++t){
        // F_p(g.active x) = f(x_{g^-1(p(i))}); original bitset engine is reused.
        Bits transformed=r4::truth(net,r4::compose(inverse(gs[t]),p));
        for(std::size_t w=0;w<base.size();++w){
            U64 delta=base[w]^transformed[w];out.each[t]+=r4::pop(delta);union_bits[w]|=delta;
            if(t==4 && delta && out.s_first==std::numeric_limits<U64>::max()){
                out.s_first=64*w+static_cast<unsigned>(__builtin_ctzll(delta));
                out.before=static_cast<int>(r4::bit(base,out.s_first));
                out.after=static_cast<int>(r4::bit(transformed,out.s_first));
            }
        }
    }
    for(U64 w:union_bits)out.combined+=r4::pop(w);return out;
}
static std::string counts_json(const Counts&c){
    std::ostringstream o;o<<"{\"per_transform_disagreement\":{";
    for(unsigned i=0;i<8;++i){if(i)o<<',';o<<r4::quote(labels[i])<<':'<<c.each[i];}
    o<<"},\"d4_union_defect_count\":"<<c.combined<<'}';return o.str();
}
static std::string patch_json(U64 x,bool descending){
    std::ostringstream o;o<<'[';
    for(int row=0;row<5;++row){
        if(row)o<<',';o<<'[';int yy=descending?4-row:row;
        for(int col=0;col<5;++col){if(col)o<<',';o<<((x>>(5*yy+col))&1);}
        o<<']';
    }o<<']';return o.str();
}
static std::string coordinates_json(U64 x){
    std::ostringstream o;o<<'[';bool first=true;
    for(unsigned j=0;j<25;++j)if((x>>j)&1){
        if(!first)o<<',';first=false;o<<'['<<int(j%5)-2<<','<<int(j/5)-2<<']';
    }o<<']';return o.str();
}
static int self_test(){
    U64 assertions=0;auto check=[&](bool b,const std::string&m){++assertions;ensure(b,m);};
    for(int side:{3,5}){
        auto gs=active_group(side);int n=side*side;auto id=r4::identity(n);
        for(const auto&g:gs){check(r4::compose(g,inverse(g))==id,"inverse failed");for(const auto&h:gs)check(std::find(gs.begin(),gs.end(),r4::compose(g,h))!=gs.end(),"closure failed");}
        auto old=r4::d4(n);
        for(const auto&g:gs)check(std::find(old.begin(),old.end(),g)!=old.end(),"adapter group differs from old D4 set");
        for(int j=0;j<n;++j)check(transform_input(U64(1)<<j,gs[1])==(U64(1)<<gs[1][j]),"active singleton orientation");
    }
    auto gs=active_group(3);
    for(const std::string&text:{std::string("{9,1,0,1,2,2,0}(0)"),
        std::string("{9,1,2,1,2,2,0}([9,10]0,3,1)([11,12]10,8,2)(12)")}){
        auto net=r4::parse(text);
        for(const auto&p:std::vector<Perm>{r4::identity(9),{4,2,0,8,6,1,3,5,7}}){
            auto base=r4::truth(net,p);auto c=evaluate_transforms(net,p,base,gs);
            std::array<U64,8>direct{};U64 united=0;
            for(U64 x=0;x<512;++x){
                int before=scalar_spatial(net,p,x);check(int(r4::bit(base,x))==before,"base bits/scalar differ");
                bool any=false;
                for(unsigned t=1;t<8;++t){
                    U64 tx=transform_input(x,gs[t]);bool delta=before!=scalar_spatial(net,p,tx);
                    direct[t]+=delta;any|=delta;
                }united+=any;
            }
            check(c.each==direct&&c.combined==united,"active bitset/scalar complete small mismatch");
        }
    }
    std::cout<<"{\"status\":\"PASS\",\"mode\":\"deterministic_adapter_self_test\",\"assertions\":"<<assertions<<",\"largest_complete_truth_inputs\":9,\"scientific_inputs_read\":false}\n";return 0;
}
static int formal(int argc,char**argv){
    ensure(argc==6,"usage: NETLIST SAVED_TRUTH MAPPING_CSV OUT_ACCEPTANCE_JSON OUT_COUNTEREXAMPLE_JSON");
    fs::path output=argv[4],counter=argv[5];ensure(!fs::exists(output)&&!fs::exists(counter),"formal output already exists");
    auto net=r4::load_net(argv[1]);ensure(net.n==25,"requires fixed 25-input network");
    auto p=r4::parse_perm(argv[3],25),id=r4::identity(25);auto gs=active_group(5);
    auto saved=saved_truth(argv[2]);auto original=r4::truth(net,id);
    U64 identity_delta=0;for(std::size_t w=0;w<saved.size();++w)identity_delta+=r4::pop(saved[w]^original[w]);
    if(identity_delta){
        std::string raw="{\"status\":\"IMPLEMENTATION_OR_CONVENTION_INVALID\",\"reason\":\"saved_truth_identity_mismatch\",\"mismatch_count\":"+std::to_string(identity_delta)+"}\n";
        save_new(output,raw);save_new(counter,"{\"status\":\"NOT_REACHED\"}\n");std::cout<<raw;return 21;
    }
    Counts orig=evaluate_transforms(net,id,original,gs);
    auto mapped=r4::truth(net,p);Counts changed=evaluate_transforms(net,p,mapped,gs);
    bool rotations=changed.each[1]==0&&changed.each[2]==0&&changed.each[3]==0;
    bool reflections=true;for(unsigned t=4;t<8;++t)reflections &= changed.each[t]>0;
    bool patch_ok=false;std::string witness;
    if(changed.s_first!=std::numeric_limits<U64>::max()){
        U64 x=changed.s_first,y=transform_input(x,gs[4]);
        patch_ok=scalar_spatial(net,p,x)==changed.before&&scalar_spatial(net,p,y)==changed.after&&changed.before!=changed.after;
        std::ostringstream c;
        c<<"{\"status\":"<<r4::quote(patch_ok?"COUNTEREXAMPLE_CONFIRMED":"IMPLEMENTATION_OR_CONVENTION_INVALID")
         <<",\"transform\":\"s\",\"active_formula\":\"s(x,y)=(x,-y)\",\"first_input_index\":"<<x
         <<",\"transformed_input_index\":"<<y<<",\"bit_index_formula\":\"(x+2)+5*(y+2)\""
         <<",\"original_output\":"<<changed.before<<",\"reflected_output\":"<<changed.after
         <<",\"ones_coordinates\":"<<coordinates_json(x)<<",\"reflected_ones_coordinates\":"<<coordinates_json(y)
         <<",\"matrix_rows_y_increasing\":"<<patch_json(x,false)
         <<",\"matrix_rows_y_descending_cartesian_display\":"<<patch_json(x,true)
         <<",\"reflected_matrix_rows_y_descending_cartesian_display\":"<<patch_json(y,true)
         <<",\"display_columns_x\":[-2,-1,0,1,2],\"display_rows_y\":[2,1,0,-1,-2]"
         <<",\"mapping_direction\":\"core_pin_to_spatial_index\",\"mapping\":"<<array(p)
         <<",\"scope\":\"The fixed C4 wiring fails this reflection; no image-quality claim.\"}\n";witness=c.str();
    }else witness="{\"status\":\"NO_REFLECTION_COUNTEREXAMPLE\",\"transform\":\"s\"}\n";
    std::string status=!rotations?"C4_WITNESS_VERIFICATION_FAILED":
        (!reflections||changed.combined==0||!patch_ok?"IMPLEMENTATION_OR_CONVENTION_INVALID":"C4_ACCEPTED");
    std::ostringstream o;
    o<<"{\"schema\":\"r4-fixed-c4-acceptance-v1\",\"status\":"<<r4::quote(status)
     <<",\"N\":"<<(U64(1)<<25)<<",\"n\":25,\"formal_invocations\":1,\"wirings_evaluated\":2"
     <<",\"engine\":\"original r4::truth and r4::scalar included without modification\""
     <<",\"coordinate_index\":\"(x+2)+5*(y+2)\",\"active_action\":\"(g.x)_v=x_(g^-1.v)\""
     <<",\"r\":\"(x,y)->(-y,x)\",\"s\":\"(x,y)->(x,-y)\",\"transform_order\":\"e,r,r2,r3,s,rs,r2s,r3s\""
     <<",\"mapping_direction\":\"core_pin_to_spatial_index\",\"mapping\":"<<array(p)
     <<",\"inverse_mapping\":"<<array(inverse(p))<<",\"saved_truth_identity_mismatch_count\":"<<identity_delta
     <<",\"identity\":"<<counts_json(orig)<<",\"c4\":"<<counts_json(changed)
     <<",\"assertions\":{\"all_three_rotation_counts_zero\":"<<(rotations?"true":"false")
     <<",\"all_four_reflection_counts_positive\":"<<(reflections?"true":"false")
     <<",\"d4_union_defect_positive\":"<<(changed.combined>0?"true":"false")
     <<",\"first_s_counterexample_scalar_bitset_agree\":"<<(patch_ok?"true":"false")<<"}}\n";
    save_new(output,o.str());save_new(counter,witness);std::cout<<o.str();
    return status=="C4_ACCEPTED"?0:(rotations?21:20);
}
int main(int argc,char**argv){
    try{if(argc==2&&std::string(argv[1])=="--self-test")return self_test();return formal(argc,argv);}
    catch(const std::exception&e){std::cout<<"{\"status\":\"IMPLEMENTATION_OR_CONVENTION_INVALID\",\"error\":"<<r4::quote(e.what())<<"}\n";return 2;}
}
