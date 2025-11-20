#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <stdexcept>
using namespace std;

struct Production {
    string lhs;
    vector<string> rhs;
};

struct Item {
    int prod; // production index
    int dot;  // position of dot (0..rhs.size())
    bool operator==(Item const &o) const { return prod==o.prod && dot==o.dot; }
};

struct ItemHash { size_t operator()(Item const &it) const noexcept { return (it.prod<<16) ^ it.dot; } };

using State = vector<Item>;

static string join(const vector<string>& v, const string &sep=" "){
    string s;
    for(size_t i=0;i<v.size();++i){ if(i) s+=sep; s+=v[i]; }
    return s;
}

// read grammar file
vector<Production> read_grammar(const string &path, set<string> &nonterminals, set<string> &terminals){
    ifstream in(path);
    if(!in) throw runtime_error("Could not open grammar file: " + path);
    vector<Production> prods;
    string line;
    while(getline(in,line)){
        // strip comments after #
        auto posc = line.find('#');
        if(posc!=string::npos) line = line.substr(0,posc);
        // trim
        auto trim = [](string s){
            size_t a = s.find_first_not_of(" \t\r\n");
            if(a==string::npos) return string();
            size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a,b-a+1);
        };
        line = trim(line);
        if(line.empty()) continue;
        // expect format: A -> alpha | beta
        auto arrow = line.find("->");
        if(arrow==string::npos) throw runtime_error("Invalid production (missing ->): " + line);
        string lhs = trim(line.substr(0,arrow));
        nonterminals.insert(lhs);
        string rest = line.substr(arrow+2);
        // split alternatives by | 
        stringstream ss(rest);
        string alt;
        vector<string> alts;
        // manual split by | because tokens might have spaces
        size_t start = 0;
        while(start < rest.size()){
            size_t bar = rest.find('|', start);
            if(bar==string::npos){ alts.push_back(trim(rest.substr(start))); break; }
            alts.push_back(trim(rest.substr(start, bar-start)));
            start = bar+1;
        }
        for(auto &a: alts){
            Production p;
            p.lhs = lhs;
            // split a by spaces
            stringstream s2(a);
            string tok;
            while(s2 >> tok){
                if(tok=="eps") continue; // epsilon
                p.rhs.push_back(tok);
            }
            prods.push_back(p);
        }
    }
    // collect terminals: any symbol that is not a nonterminal
    for(auto &p: prods){
        for(auto &s: p.rhs) if(nonterminals.count(s)==0) terminals.insert(s);
    }
    terminals.insert("$");
    return prods;
}

// Item helpers
string symbol_after_dot(const Item &it, const vector<Production> &prods){
    const auto &p = prods[it.prod];
    if(it.dot < (int)p.rhs.size()) return p.rhs[it.dot];
    return string();
}

bool is_dot_at_end(const Item &it, const vector<Production> &prods){
    return it.dot >= (int)prods[it.prod].rhs.size();
}

// closure
State closure(const State &I, const vector<Production> &prods, const set<string> &nonterminals){
    unordered_set<Item, ItemHash> C(I.begin(), I.end());
    queue<Item> q;
    for(auto &it: I) q.push(it);
    while(!q.empty()){
        Item it = q.front(); q.pop();
        string a = symbol_after_dot(it, prods);
        if(a.empty()) continue;
        if(nonterminals.count(a)==0) continue;
        // for each production A -> gamma, add [A -> . gamma]
        for(int i=0;i<(int)prods.size();++i){
            if(prods[i].lhs == a){
                Item nit{i,0};
                if(!C.count(nit)) { C.insert(nit); q.push(nit); }
            }
        }
    }
    State out(C.begin(), C.end());
    // sort deterministically
    sort(out.begin(), out.end(), [](const Item &x, const Item &y){ if(x.prod!=y.prod) return x.prod < y.prod; return x.dot < y.dot; });
    return out;
}

// goto(I, X)
State go_to(const State &I, const string &X, const vector<Production> &prods, const set<string> &nonterminals){
    State J;
    for(auto &it: I){
        string a = symbol_after_dot(it, prods);
        if(!a.empty() && a==X){
            J.push_back({it.prod, it.dot+1});
        }
    }
    return closure(J, prods, nonterminals);
}

int state_index(const vector<State> &C, const State &S){
    for(size_t i=0;i<C.size();++i){
        if(C[i].size()!=S.size()) continue;
        bool ok = true;
        for(size_t j=0;j<S.size();++j) if(!(C[i][j]==S[j])) { ok=false; break; }
        if(ok) return (int)i;
    }
    return -1;
}

struct Action { // either shift to state, reduce prod, accept, or error
    enum Type {ERR, SHIFT, REDUCE, ACCEPT} type = ERR;
    int val = -1; // state or production index
};

int main(int argc, char **argv){
    if(argc < 3){
        cerr<<"Usage: "<<argv[0]<<" <grammar-file> <input-tokens (quoted, space-separated)>\n";
        cerr<<"Example: ./lr0_parser grammar.txt \"a a a\"\n";
        return 1;
    }
    string grammarFile = argv[1];
    string inputStr;
    // join remaining argv into tokens string
    for(int i=2;i<argc;++i){ if(i>2) inputStr += " "; inputStr += argv[i]; }

    set<string> nonterminals, terminals;
    vector<Production> prods = read_grammar(grammarFile, nonterminals, terminals);
    if(prods.empty()) { cerr<<"No productions read\n"; return 1; }

    // augment grammar: S' -> S (S is first production's LHS)
    string S0 = prods[0].lhs;
    Production aug; aug.lhs = S0 + "'"; aug.rhs = {S0};
    prods.insert(prods.begin(), aug);
    nonterminals.insert(aug.lhs);

    // recompute terminals (ensure $ present)
    terminals.insert("$");

    // build canonical collection
    State I0; I0.push_back({0,0});
    State c0 = closure(I0, prods, nonterminals);
    vector<State> C; C.push_back(c0);
    // symbol set: union of terminals and nonterminals except eps
    set<string> symbols;
    for(auto &t: terminals) symbols.insert(t);
    for(auto &nt: nonterminals) symbols.insert(nt);

    bool changed = true;
    while(changed){
        changed = false;
        for(size_t i=0;i<C.size();++i){
            // for each grammar symbol X
            set<string> nextSymbols;
            for(auto &it: C[i]){
                string a = symbol_after_dot(it, prods);
                if(!a.empty()) nextSymbols.insert(a);
            }
            for(auto &X: nextSymbols){
                State g = go_to(C[i], X, prods, nonterminals);
                if(g.empty()) continue;
                if(state_index(C, g) == -1){ C.push_back(g); changed = true; }
            }
        }
    }

    // build ACTION and GOTO tables
    int N = (int)C.size();
    // Action: vector<map<terminal,Action>>; Goto: vector<map<nonterminal,int>>
    vector<unordered_map<string,Action>> ACTION(N);
    vector<unordered_map<string,int>> GOTO(N);

    for(int i=0;i<N;++i){
        for(auto &it: C[i]){
            if(!is_dot_at_end(it, prods)){
                string a = symbol_after_dot(it, prods);
                // if a terminal and goto(i,a)=j => ACTION[i][a]=shift j
                State g = go_to(C[i], a, prods, nonterminals);
                if(!g.empty()){
                    int j = state_index(C, g);
                    if(terminals.count(a)){
                        Action act; act.type = Action::SHIFT; act.val = j;
                        auto &cur = ACTION[i][a];
                        if(cur.type==Action::ERR) cur = act;
                        else if(cur.type!=act.type || cur.val!=act.val){
                            // conflict: prefer SHIFT over REDUCE
                            if(cur.type==Action::REDUCE && act.type==Action::SHIFT){ cur = act; }
                            // otherwise keep existing (simple resolution)
                        }
                    } else if(nonterminals.count(a)){
                        GOTO[i][a] = j;
                    }
                }
            } else {
                // dot at end: reduce by production it.prod (if prod != 0)
                if(it.prod == 0){
                    // augmented production S' -> S . => accept on $
                    Action acc; acc.type = Action::ACCEPT; acc.val = -1;
                    ACTION[i]["$"] = acc;
                } else {
                    Action r; r.type = Action::REDUCE; r.val = it.prod;
                    // for LR(0) naive: place reduce on all terminals (may cause conflicts)
                    for(auto &t: terminals){
                        auto &cur = ACTION[i][t];
                        if(cur.type==Action::ERR) cur = r;
                        else if(cur.type!=r.type || cur.val!=r.val){
                            // conflict resolution: prefer SHIFT over REDUCE
                            if(cur.type==Action::SHIFT) { /* keep shift */ }
                            else if(cur.type==Action::REDUCE){
                                // reduce/reduce: choose smallest production index
                                if(r.val < cur.val) cur = r;
                            }
                        }
                    }
                }
            }
        }
    }

    // show summary
    cout<<"Grammar productions:\n";
    for(size_t i=0;i<prods.size();++i){
        cout<<"  "<<i<<": "<<prods[i].lhs<<" -> ";
        if(prods[i].rhs.empty()) cout<<"eps";
        else cout<<join(prods[i].rhs);
        cout<<"\n";
    }
    cout<<"\nStates ("<<N<<"):\n";
    for(int i=0;i<N;++i){
        cout<<"I"<<i<<":\n";
        for(auto &it: C[i]){
            cout<<"  ["<<it.prod<<"] "<<prods[it.prod].lhs<<" -> ";
            for(int k=0;k<(int)prods[it.prod].rhs.size();++k){
                if(k==it.dot) cout<<". ";
                cout<<prods[it.prod].rhs[k]<<" ";
            }
            if(it.dot == (int)prods[it.prod].rhs.size()) cout<<". ";
            cout<<"\n";
        }
        cout<<"\n";
    }

    // print ACTION table (terminals)
    cout<<"ACTION table (terminals):\n";
    // collect list of terminals sorted
    vector<string> termList(terminals.begin(), terminals.end());
    sort(termList.begin(), termList.end());
    cout<<"state";
    for(auto &t: termList) cout<<"\t"<<t;
    cout<<"\n";
    for(int i=0;i<N;++i){
        cout<<i;
        for(auto &t: termList){
            cout<<"\t";
            auto it = ACTION[i].find(t);
            if(it==ACTION[i].end()) cout<<".";
            else {
                auto a = it->second;
                if(a.type==Action::SHIFT) cout<<"s"<<a.val;
                else if(a.type==Action::REDUCE) cout<<"r"<<a.val;
                else if(a.type==Action::ACCEPT) cout<<"acc";
                else cout<<".";
            }
        }
        cout<<"\n";
    }
    cout<<"\nGOTO table (nonterminals):\n";
    vector<string> ntList(nonterminals.begin(), nonterminals.end());
    sort(ntList.begin(), ntList.end());
    cout<<"state";
    for(auto &nt: ntList) cout<<"\t"<<nt;
    cout<<"\n";
    for(int i=0;i<N;++i){
        cout<<i;
        for(auto &nt: ntList){
            cout<<"\t";
            auto it = GOTO[i].find(nt);
            if(it==GOTO[i].end()) cout<<"."; else cout<<it->second;
        }
        cout<<"\n";
    }

    // Parse input tokens
    // split inputStr by spaces
    vector<string> inputTokens;
    {
        stringstream s(inputStr);
        string tok;
        while(s >> tok) inputTokens.push_back(tok);
    }
    inputTokens.push_back("$");

    cout<<"\nParsing input: "<<join(inputTokens)<<"\n\n";
    // stack of states
    vector<int> stackStates; stackStates.push_back(0);
    size_t ip = 0;
    while(true){
        int s = stackStates.back();
        string a = inputTokens[ip];
        Action act = ACTION[s].count(a) ? ACTION[s][a] : Action();
        if(act.type==Action::SHIFT){
            cout<<"shift '"<<a<<"' -> state "<<act.val<<"\n";
            stackStates.push_back(act.val);
            ip++;
        } else if(act.type==Action::REDUCE){
            auto &p = prods[act.val];
            cout<<"reduce by "<<act.val<<": "<<p.lhs<<" -> ";
            if(p.rhs.empty()) cout<<"eps"; else cout<<join(p.rhs);
            cout<<"\n";
            int m = (int)p.rhs.size();
            for(int k=0;k<m;++k) stackStates.pop_back();
            int t = stackStates.back();
            if(GOTO[t].count(p.lhs)==0){ cerr<<"Error: no GOTO for state "<<t<<" and nonterminal "<<p.lhs<<"\n"; return 1; }
            int g = GOTO[t][p.lhs];
            stackStates.push_back(g);
            cout<<"goto state "<<g<<"\n";
        } else if(act.type==Action::ACCEPT){
            cout<<"Input accepted (ACCEPT)\n";
            break;
        } else {
            cerr<<"Parse error at token '"<<a<<"' (state "<<s<<")\n";
            return 1;
        }
    }

    return 0;
}
