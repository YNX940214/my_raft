//
// Created by ynx on 2020-02-09.
//

#include "RaftServer.h"
#include "rpc/rpc_to_string.h"

RaftServer::RaftServer(io_service &loop, const string &_ip, int _port, string config_path, StateMachine *sm) :
        ip_(_ip),
        port_(_port),
        entries_(_port),
        server_(_ip, _port),
        ioContext(loop),
        term_(0),
        listen_point_(tcp::v4(), port_),
        state_(follower),
        already_voted_(false),
        candidate_timer_(loop),
        commit_index_(-1),
        network_(loop, listen_point_, std::bind(&RaftServer::reactToIncomingMsg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
        winned_votes_(0),
        config_path_(config_path),
        smc_(sm, &entries_, ioContext, this) {
    load_config_from_file();
    for (const auto &server_tuple: configuration_) {
        nextIndex_[server_tuple] = 0; // -1 not 0, because at the beginning, entries[0] raise Exception,
        Follower_saved_index[server_tuple] = -1;
        current_rpc_lsn_[server_tuple] = 0;
        follower_rpc_lsn_[server_tuple] = -1;
        auto sp = make_shared<Timer>(loop);
        retry_timers_.insert({server_tuple, sp});
    }
}

void RaftServer::run() {
    network_.startAccept();
    int waiting_counts = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(random_candidate_expire()));
    if (waiting_counts != 0) {
        throw_line("when server starts this can't be 0");
    } else {
        //todo 这部分代码有很多地方冗余了，可以考虑用bind做抽象
        candidate_timer_.async_wait([this](const boost::system::error_code &error) {
            if (error) {
                if (error == boost::asio::error::operation_aborted) {
                    //throw_line("所有的handler通过set timer特殊值的方式取消，应该不可能出现operation_aborted");
                    /*
                     * 上面注释掉的代码抛出异常以检测，但是trans2C->cancel_all_timers—> expires 就会取消定时器（虽然它把expire time设置为min_date_time），所以上面总会抛异常，真确的做法是 return
                     */
                    return;
                } else {
                    std::ostringstream oss;
                    oss << "handler in run's candidate_timer, error: " << error.message();
                    string str = oss.str();
                    throw_line(str);
                }
            } else {
                auto expire_time = candidate_timer_.expires_at();
                if (expire_time == boost::posix_time::min_date_time) {
                    Log_trace << "handler in run()'s candidate_timer_ is canceled by setting its timer's expiring time to min_date_time";
                    return;                         // canceled，we can't do anything
                } else {
                    trans2C();
                }
            }
        });
    }
    ioContext.run();
}


//mock 一下先
void RaftServer::load_config_from_file() {
    std::ifstream in(config_path_);
    if (!in) {
        cout << "Cannot open config file: " << config_path_ << endl;
        throw_line("read config file failed");
    }

    char str[255];
    while (in) {
        in.getline(str, 255);
        if (string(str).size() == 0)  //丑陋，不过在同一台电脑上在clion上运行居然和在bash里面运行结果一样，奇怪
            break;
        auto vec = split_str_boost(str, ':');
        cout << "the size is: " << vec.size() << endl;
        if (vec.size() != 2) {
            throw_line(string("failed to read the config file line: ") + str);
        }
        int port = std::atoi(vec[1].c_str());
        cout << "read config line: " << vec[0] << ":" << vec[1] << endl;
        auto server = std::make_tuple(vec[0], port);
        configuration_.push_back(server);
    }
    in.close();
    N_ = configuration_.size();
    majority_ = N_ / 2 + 1;
}

void RaftServer::cancel_all_timers() {
    /*
     * asio存在这样一种情况，timer1在2s后超时，timer2在3s后超时，timer1的handler会取消timer2，但是如果sleep了5s，那么timer2的handler将无法取消timer3的handler，cancel函数返回的数目是0
     * 由于我的代码中逻辑在取消了handler被取消之后，不需要做任何事情，所以我在写代码的时候要保证一点的是，保证一个handler是一定能被取消的，并且（由于这个代码的特殊逻辑）被取消的handler不会做任何事！
     * 保证一个handler是一定能被取消的，并且（由于这个代码的特殊逻辑）被取消的handler不会做任何事！
     * 保证一个handler是一定能被取消的，并且（由于这个代码的特殊逻辑）被取消的handler不会做任何事！
     */
    Log_trace << "begin";

//        int canceled_number = candidate_timer_.cancel();
//        {
//            std::ostringstream oss;
//            oss << canceled_number << " handlers was canceled on the candidate_timer_" << endl;
//            string s = oss.str();
//            Log_debug << s;
//        }
    //对于candidate_timer_上面那种方法通过取消的方法并不安全
    candidate_timer_.expires_at(boost::posix_time::min_date_time);

    /*
     * since cancel function can not cancel the handlers that is already expired, see boost_tiemr_bug(2).cpp, we reference
     *  https://stackoverflow.com/questions/43168199/cancelling-boost-asio-deadline-timer-safely
     *  to cancel the expired handlers by setting its timer's expire time at a special value,
     *  and when executed the handlers will check if the special value is set, if set, that means the handler has been canceled.
     */
    for (auto pair : retry_timers_) {
        pair.second->get_core().expires_at(boost::posix_time::min_date_time);
    }
}


void RaftServer::trans2F(int term) {
    Log_info << "begin, term: " << term;
    cout << __FUNCTION__ << endl;
    cancel_all_timers();
    winned_votes_ = 0;
    update_term(term);
    already_voted_ = false;
    state_ = follower;
    int waiting_count = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(random_candidate_expire()));
    if (waiting_count != 0) {
        throw_line("trans2F在一开始已经执行了cancel_all_timers函数，所以这里不应该有waiting_count=0");
    }
    candidate_timer_.async_wait([this](const boost::system::error_code &error) {
        Log_trace << "handler in trans2F's candidate_timers expired, error: " << error.message();
        if (error) {
            if (error == boost::asio::error::operation_aborted) {
                //throw_line("所有的handler通过set timer特殊值的方式取消，应该不可能出现operation_aborted");
                /*
                 * 上面注释掉的代码抛出异常以检测，但是trans2C->cancel_all_timers—> expires 就会取消定时器（虽然它把expire time设置为min_date_time），所以上面总会抛异常，真确的做法是 return
                 */
                return;
            } else {
                std::ostringstream oss;
                oss << "handler in trans2F's candidate_timers, error: " << error.message();
                string str = oss.str();
                throw_line(str);
            }
        } else {
            auto expire_time = candidate_timer_.expires_at();
            if (expire_time == boost::posix_time::min_date_time) {
                throw_line("逻辑错误，这里是不可能出现的，因为这个情景（trans2C未能即使取消h1，（然后RV->hook h2）h1运行)被waiting_counts != 0 完全覆盖了，如果抛出这个异常，说明出现了新的bug");
//                    Log_trace << "handler in trans2F's candidate_timers is canceled by setting its timer's expiring time to min_date_time";
//                    return;                         // canceled，we can't do anything
            } else {
                trans2C();
            }
        }
    });
}

void RaftServer::trans2P() {
    Log_info << "begin";
    cout << __FUNCTION__ << endl;
    cancel_all_timers();
    state_ = primary;

    int entry_size = entries_.size();
    Log_debug << "entry size: " << entry_size;
    for (const auto &server: configuration_) {
        nextIndex_[server] = entry_size;
        Follower_saved_index[server] = -1;
    }

    for (const auto &server : configuration_) {
        int port = std::get<1>(server);
        if (port != port_) {
            AE(server);
        }
    }
}

void RaftServer::update_term(int term) {
    if (term > term_) {
        Log_info << "term grows from " << term_ << " to " << term;
        reset_both_rpc_lsn();
        term_ = term;
    } else if (term_ == term) {

    } else {
        throw_line("term_ can't never decreases");
    }
}

void RaftServer::reset_both_rpc_lsn() {
    Log_debug << "begin";
    for (const auto &server_tuple: configuration_) {
        current_rpc_lsn_[server_tuple] = 0;
        follower_rpc_lsn_[server_tuple] = -1; //-1 smaller than 0, so we can ignore rpc if new lsn is equal to local stored lsn
    }
}

void RaftServer::trans2C() {
    Log_info << "begin";
    cout << __FUNCTION__ << endl;
    /*
     * candidate can trans 2 candidate, we need to cancel the timer
     * 刚跑起来的时候这里有个bug，rv的重传超时设置成了秒，这导致trans2candidate的时候，rv的timer还没有超时，所以再次set timer expire的时候检查hook数目不为0，属于一个被遗漏的bug，但找到后修复了，但是这
     */
    cancel_all_timers();
    update_term(term_ + 1);
    Log_info << "current term: " << term_;
    state_ = candidate;
    winned_votes_ = 1;
    //设置下一个term candidate的定时器
    int waiting_count = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(random_candidate_expire()));
    if (waiting_count == 0) {
        //完全可能出现这种情况，如果是cb_trans2_candidate的定时器自然到期，waiting_count就是0
        //leave behind
    } else {
        throw_line("trans2C只能是timer_candidate_expire自然到期触发，所以不可能有waiting_count不为0的情况，否则就是未考虑的情况");
    }
    candidate_timer_.async_wait([this](const boost::system::error_code &error) {
        Log_trace << "handler in trans2C's candidate_timers expired, error: " << error.message();
        if (error) {
            if (error == boost::asio::error::operation_aborted) {
                //throw_line("所有的handler通过set timer特殊值的方式取消，应该不可能出现operation_aborted");
                /*
                 * 上面注释掉的代码抛出异常以检测，但是trans2C->cancel_all_timers—> expires 就会取消定时器（虽然它把expire time设置为min_date_time），所以上面总会抛异常，真确的做法是 return
                 */
                return;
            } else {
                std::ostringstream oss;
                oss << "handler in trans2C's candidate_timers, error: " << error.message();
                string str = oss.str();
                throw_line(str);
            }
        } else {
            auto expire_time = candidate_timer_.expires_at();
            if (expire_time == boost::posix_time::min_date_time) {
                throw_line("逻辑错误，这里是不可能出现的，因为这个情景（trans2C未能即使取消h1，（然后RV->hook h2）h1运行)被waiting_counts != 0 完全覆盖了，如果抛出这个异常，说明出现了新的bug");
//                    Log_trace << "handler in trans2C's candidate_timers is canceled by setting its timer's expiring time to min_date_time";
//                    return;                         // canceled，we can't do anything
            } else {
                trans2C();
            }
        }
    });
    for (auto &server : configuration_) {
        int port = std::get<1>(server);
        if (port != port_) {
            RV(server);
        }
    }
}


string RaftServer::build_rpc_ae_string(const tuple<string, int> &server) {
    Log_trace << "begin: server: " << server2str(server);
    AppendEntryRpc rpc;
    int server_current_rpc_lsn = current_rpc_lsn_[server];
    current_rpc_lsn_[server] = server_current_rpc_lsn + 1;
    rpc.set_lsn(server_current_rpc_lsn);
    rpc.set_term(term_);
    //这里我们在主比较follower_saved_index和commit_index，发送其中更小的那个，从在收到的时候不判断commit_index与entry size的大小
    //做一个鱼雷
    int commit_index_to_send = commit_index_;
    {
        int F_saved = Follower_saved_index[server];
        if (commit_index_ > F_saved) {
            commit_index_to_send = F_saved;
        }
    }
    rpc.set_commit_index(commit_index_to_send);
    rpc.set_ip(ip_);
    rpc.set_port(port_);

    int index_to_send = nextIndex_[server];

    // set prevlog_term
    if (index_to_send > 0) {
        const rpc_Entry &prev_entry = entries_.get(index_to_send - 1);
        rpc.set_prelog_term(prev_entry.term());
    } else {
        rpc.set_prelog_term(-1);
    }

    // set entry if any
    if (index_to_send == entries_.size()) {
        //empty rpc as HB
    } else {
        const rpc_Entry &entry = entries_.get(index_to_send);
        rpc_Entry *entry_to_send = rpc.add_entry();
        entry_to_send->set_term(entry.term());
        entry_to_send->set_msg(entry.msg());
        entry_to_send->set_index(entry.index());
    }
    string msg;
    rpc.SerializeToString(&msg);
    return msg;
}

void RaftServer::AE(const tuple<string, int> &server) {
    Log_debug << "begin";
    shared_ptr<Timer> t1 = retry_timers_[server];
    string ae_rpc_str = build_rpc_ae_string(server);
    //考虑如下场景，发了AE1,index为100,然后定时器到期重发了AE2,index为100，这时候收到ae1的resp为false，将index--,如果ae2到F，又触发了一次resp，这两个rpc的请求一样，resp一样，但是P收到两个一样的resp会将index--两次，所以需要rpc_lsn的存在
    network_.make_rpc_call(APPEND_ENTRY, server, ae_rpc_str);

    int waiting_counts = t1->get_core().expires_from_now(boost::posix_time::milliseconds(random_ae_retry_expire()));
    if (waiting_counts != 0) {
        throw_line("should be zero, a timer can't be interrupt by network, but when AE is called, there should be no hooks on the timer");
    } else {
        t1->get_core().async_wait([this, t1, server](const boost::system::error_code &error) {
            Log_trace << "handler in AE's retry timer to " << server2str(server) << " expired, error: " << error.message();
            if (error) {
                if (error == boost::asio::error::operation_aborted) {
                    //throw_line("所有的handler通过set timer特殊值的方式取消，应该不可能出现operation_aborted");
                    /*
                     * 上面注释掉的代码抛出异常以检测，但是trans2C->cancel_all_timers—> expires 就会取消定时器（虽然它把expire time设置为min_date_time），所以上面总会抛异常，真确的做法是 return
                     */
                    return;
                } else {
                    std::ostringstream oss;
                    oss << "handler in AE's retry timer to " << server2str(server) << ", error: " << error.message();
                    string str = oss.str();
                    throw_line(str);
                }
            } else {
                auto expire_time = t1->get_core().expires_at();
                if (expire_time == boost::posix_time::min_date_time) {
                    Log_trace << "handler in AE's retry_timer_ to " << server2str(server) << " is canceled by setting its timer's expiring time to min_date_time";
                    return;                         // canceled，we can't do anything
                } else {
                    /* situtations:
                     * One: if the ae is lost, AE(server) will get the same index to send to followers
                     * Two: P's index is [0,1], F's index is [0], P send ae(index=1) to F, resp_ae returns, the ok is true, the primary found (currentIndex[server]+1 == entries_.size()), then P won't cancel the timer but only add the currentIndex to 2, when the timer expires, it can found 2==entries_.size(), so send an empty AE
                     * Three: P's index is [0,1,2], F's index is [0], P send ae(index=1) to F, resp_ae returns, the ok is true, the primary found (currentIndex[server]+1 "that's 2" < entries_.size() "that's 3" ), so P cancel the timer, add the currentIndex, and immediately send the next AE
                     * */
                    AE(server); //this will re-get the index to send, if before the timer expires, the resp returns and ok, and the resp is the ae of the last index, then we will not
                }
            }
        });
    }
}

string RaftServer::build_rpc_rv_string(const tuple<string, int> &server) {
    Log_trace << "begin: server: " << server2str(server);
    int server_current_rpc_lsn = current_rpc_lsn_[server];
    current_rpc_lsn_[server] = server_current_rpc_lsn + 1;
    RequestVoteRpc rpc;
    rpc.set_lsn(server_current_rpc_lsn);
    rpc.set_term(term_);
    rpc.set_latest_index(entries_.size() - 1);
    rpc.set_ip(ip_);
    rpc.set_port(port_);
    string msg;
    rpc.SerializeToString(&msg);
    return msg;
}

void RaftServer::RV(const tuple<string, int> &server) {
    Log_debug << "begin: server: " << server2str(server);
    shared_ptr<Timer> timer = retry_timers_[server];

    string rv_rpc_str = build_rpc_rv_string(server);
    network_.make_rpc_call(REQUEST_VOTE, server, rv_rpc_str);
    int waiting_counts = timer->get_core().expires_from_now(boost::posix_time::milliseconds(random_rv_retry_expire()));
    if (waiting_counts != 0) {
        /*
         * throw_line("if the cancel_all_timers has already canceled the RV retry, the exe stream will not come to here, as it comes here, there is something wrong");
         * 上述代码抛异常以便检查，实际为asset，因为我认为不会出现那种情况，但是回出现，情景如下：
         * handler1（RV）expire 了但是为执行，此时trans2C执行，没有成功取消handler1（虽然handler1的expire时间已超过），但是hook了handler2，现在handler1执行，检查到timer上有1个handler，就是handler2，抛异常
         */
        return;
    } else {
        timer->get_core().async_wait([this, timer, server](const boost::system::error_code &error) {
            Log_trace << "handler in RV's retry timer to " << server2str(server) << " expired, error: " << error.message();
            if (error) {
                if (error == boost::asio::error::operation_aborted) {
                    //throw_line("所有的handler通过set timer特殊值的方式取消，应该不可能出现operation_aborted");
                    /*
                     * 上面注释掉的代码抛出异常以检测，但是trans2C->cancel_all_timers—> expires 就会取消定时器（虽然它把expire time设置为min_date_time），所以上面总会抛异常，真确的做法是 return
                     */
                    return;
                } else {
                    std::ostringstream oss;
                    oss << "handler in RV's retry timer to " << server2str(server) << ", error: " << error.message();
                    string str = oss.str();
                    throw_line(str);
                }
            } else {
                auto expire_time = timer->get_core().expires_at();
                if (expire_time == boost::posix_time::min_date_time) {
                    throw_line("逻辑错误，这里是不可能出现的，因为这个情景（trans2C未能即使取消h1，（然后RV->hook h2）h1运行)被waiting_counts != 0 完全覆盖了，如果抛出这个异常，说明出现了新的bug");
//                        Log_trace << "handler in RV's retry_timer_ to " << server2str(server) << " is canceled by setting its timer's expiring time to min_date_time";
//                        return;                         // canceled，we can't do anything
                } else {
                    RV(server);
                }
            }
        });
    }
}


void RaftServer::reactToIncomingMsg(RPC_TYPE _rpc_type, const string msg, const tuple<string, int> &peer_addr) {
    Log_debug << "begin: RPC_TYPE: " << _rpc_type;
    if (_rpc_type == REQUEST_VOTE) {
        RequestVoteRpc rv;
        rv.ParseFromString(msg);
        react2rv(rv);
    } else if (_rpc_type == APPEND_ENTRY) {
        AppendEntryRpc ae;
        ae.ParseFromString(msg);
        react2ae(ae);
    } else if (_rpc_type == RESP_VOTE) {
        Resp_RequestVoteRpc resp_rv;
        resp_rv.ParseFromString(msg);
        react2resp_rv(resp_rv);
    } else if (_rpc_type == RESP_APPEND) {
        Resp_AppendEntryRpc resp_ae;
        resp_ae.ParseFromString(msg);
        react2resp_ae(resp_ae);
    } else if (_rpc_type == CLIENT_APPLY) {
        append_client_apply_to_entries(peer_addr, msg);
    } else if (_rpc_type == CLIENT_QUERY) {
        get_from_state_machine(peer_addr, msg);
    } else {
        Log_error << "unknown rpc_type";
    }
}

void RaftServer::get_from_state_machine(const tuple<string, int> &client_addr, string client_query_str) {
    smc_.get_from_state_machine(client_query_str, client_addr);
}

void RaftServer::append_client_apply_to_entries(const tuple<string, int> &client_addr, string apply_str) {
    Log_trace << "begin, client_addr: " << server2str(client_addr);
    rpc_Entry entry;
    int index = entries_.size();
    entryIndex_to_socketAddr_map_[index] = client_addr;
    entry.set_term(term_);
    entry.set_index(index);
    entry.set_msg(apply_str);
    entries_.insert(index, entry);
    Follower_saved_index[server_] = index;
}

void RaftServer::react2ae(AppendEntryRpc rpc_ae) {
    Log_debug << "begin";
    int remote_term = rpc_ae.term();
    int prelog_term = rpc_ae.prelog_term();
    int commit_index = rpc_ae.commit_index();
    int rpc_lsn = rpc_ae.lsn();
    bool is_empty_ae = rpc_ae.entry_size() == 0;
    std::tuple<string, int> server = std::make_tuple(rpc_ae.ip(), rpc_ae.port());

    // 这行错了!       assert(commitIndex <= prelog_index); 完全有可能出现commitIndex比prelog_index大的情况
    if (term_ > remote_term) {
        make_resp_ae(false, "react2ae, term_ > term", server, rpc_lsn, is_empty_ae);
        return;
    } else { //term_ <= remote_term
        if (state_ == primary && term_ == remote_term) {
            throw_line("error, one term has two primaries");
        }
        int pre_local_term = term_;
        trans2F(remote_term);//会设置定时器，所以此函数不需要设置定时器了


        //if term_ is updated in trans2F, then both lsn are clean, if not, the store follower_saved_index is still valid
        if (should_ignore_remote_rpc(server, rpc_lsn)) {
            return;
        }

        /*
         一开始想法是commitIndex直接赋值给本地commitIndex，然后本地有一个线程直接根据本第commitIndex闷头apply to stateMachine就行了（因为两者完全正交），
         但是一想不对，因为rpc的commitIndex可能比prelog_index大，假设本地和主同步的entry是0到20，21到100都是stale的entry，rpc传来一个commitIndex为50，prelog_index为80（还没走到真确的20），
         难道那个线程就无脑把到80的entry给apply了？所以本地的commitIndex只能是
         */
        int rpc_ae_entry_size = rpc_ae.entry_size();
        Log_debug << "rpc_ae_entry_size " << rpc_ae_entry_size;
        if (rpc_ae_entry_size == 0) { // empty ae, means that this primary thinks this follower has already catch up with it
            if (pre_local_term == remote_term) {  //为了解决在从跟上主的log之后，主发来空的AE不能更新从commit index，但是又不能让（本应该被回滚的主）更新commit_index
                follower_update_commit_index(commit_index, INT32_MAX); //无脑接受commit_index
            }
            make_resp_ae(true, "成功insert", server, rpc_lsn, is_empty_ae);
            return;
        } else if (rpc_ae_entry_size == 1) {
            const raft_rpc::rpc_Entry &entry = rpc_ae.entry(0);
            int prelog_index = entry.index() - 1;
            if (prelog_term == -1) { //see AE, we set prelog_term to -1 as this is the index0 log of primary, the follower has to accept it
                entries_.insert(0, entry);
                // we choose the smaller one to be the commit index, the logic is in entries_.update_commit_index function
                follower_update_commit_index(commit_index, prelog_index);
                make_resp_ae(true, "成功insert", server, rpc_lsn, is_empty_ae);
                return;
            } else {
                int last_index = entries_.size() - 1;
                if (last_index < prelog_index) {
                    make_resp_ae(false, "react2ae, term is ok, but follower's last entry index is " + to_string(last_index) + ", but prelog_index is " + to_string(prelog_index), server, rpc_lsn, is_empty_ae);
                    return;
                } else { // if (last_index >= prelog_index)  注意> 和 = 的情况是一样的，（至少目前我是这么认为的）
                    int term_of_local_entry_with_prelog_index = entries_.get(prelog_index).term();
                    if (term_of_local_entry_with_prelog_index == prelog_term) {
                        entries_.insert(prelog_index + 1, entry);
                        // we choose the smaller one to be the commit index, the logic is in entries_.update_commit_index function
                        follower_update_commit_index(commit_index, prelog_index);
                        make_resp_ae(true, "成功insert", server, rpc_lsn, is_empty_ae);
                        return;
                    } else if (term_of_local_entry_with_prelog_index < prelog_term) {
                        // easy to make an example
                        make_resp_ae(false, "react2ae, term is ok, but follower's entry's term" + to_string(term_of_local_entry_with_prelog_index) + "of prelog_index " + to_string(prelog_index) + "is SMALLER than rpc's prelog_term" + to_string(prelog_term), server, rpc_lsn, is_empty_ae);
                        return;
                    } else {
                        make_resp_ae(false, "react2ae, term is ok, but follower's entry's term" + to_string(term_of_local_entry_with_prelog_index) + "of prelog_index " + to_string(prelog_index) + "is BIGGER than rpc's prelog_term" + to_string(prelog_term), server, rpc_lsn, is_empty_ae);
                        return;
                        /*
                         * 1. 5 nodes with term 1, index [2]
                         * 2. a trans P (term2), index[1 2], but copy to none
                         * 3. b trans P (term3), index[1,3], copy to c
                         * 4. a trans P (term4), and receive client, index [1,2,4], then copy to all, prelog_term is 2,prelog_index is 1,
                         * so for b and c, index 1 's term is 3, but remote prelog_term is 2, is completely ok.
                         */
                    }
                }
            }
        } else {
            throw_line("rpc_ae's entry size is bigger 1");
        }
    }
}

bool RaftServer::should_ignore_remote_rpc(const tuple<string, int> &remote, int new_lsn) {
    int prev = follower_rpc_lsn_[remote];
    Log_debug << "local stored lsn " << prev << ", remote rpc call lsn: " << new_lsn;
    if (prev >= new_lsn) {
        return true;
    } else {
        follower_rpc_lsn_[remote] = new_lsn;
        return false;
    }
}


//begin from here
void RaftServer::react2rv(RequestVoteRpc rpc_rv) {
    Log_debug << "begin";
    int remote_term = rpc_rv.term();
    int rpc_lsn = rpc_rv.lsn();
    int remote_latest_term = rpc_rv.latest_term();
    int remote_latest_index = rpc_rv.latest_index();
    std::tuple<string, int> server = std::make_tuple(rpc_rv.ip(), rpc_rv.port());

    if (term_ > remote_term) {
        make_resp_rv(false, "react2rv: term_ > remote,  write back false", server, rpc_lsn);
        return;
    } else if (term_ < remote_term) {
        trans2F(remote_term);
        //do not return, do logic
    }

    if (should_ignore_remote_rpc(server, rpc_lsn)) {
        return;  // no even speak
    }

    if (state_ == candidate || state_ == primary) {
        make_resp_rv(false, "react2rv: term_ = remote,  write back false", server, rpc_lsn);
    } else {
        if (!already_voted_) {  //这个if针对 term_ == term 的情况，只有一票
            bool should_vote = false;
            if (entries_.size() == 0) { //只管投票
                should_vote = true;
            } else {
                const rpc_Entry &lastEntry = entries_.get(entries_.size() - 1);
                if (lastEntry.term() > remote_term) {
                    should_vote = false;
                } else if (lastEntry.term() < remote_term) {
                    should_vote = true;
                } else {
                    if (lastEntry.index() > remote_latest_index) { //todo deep think，why this will work，然后我就吃饭去了
                        should_vote = false;
                    } else {
                        should_vote = true;
                    }
                }
            }
            if (should_vote) {
                make_resp_rv(true, "react2rv: A vote is send", server, rpc_lsn);
                already_voted_ = true;
            } else {
                make_resp_rv(false, "react2rv: A_vote is denied for primary doesn't have newer log", server, rpc_lsn);
            }
        } else {
            make_resp_rv(false, "react2rv: No votes left this term", server, rpc_lsn);
        }
    }
}

void RaftServer::react2resp_ae(Resp_AppendEntryRpc &resp_ae) {
    Log_debug << "begin";
    int remote_term = resp_ae.term();
    std::tuple<string, int> server = std::make_tuple(resp_ae.ip(), resp_ae.port());

    if (term_ < remote_term) {
        trans2F(remote_term);
    } else if (term_ > remote_term) {
        //do nothing, even primary with the correct rpc_lsn, can't do anything for safety, not even response
    } else {
        if (state_ == primary) {
            primary_deal_resp_ae_with_same_term(server, resp_ae);
        } else {
            throw_line("不可能的情况，主ae，收到resp_ae的remote_term与term_相等，但是state不为主，说明一个term出现了两个主，错误");
        }
    }
}

void RaftServer::react2resp_rv(Resp_RequestVoteRpc &resp_rv) {
    Log_debug << "begin";
    std::tuple<string, int> server = std::make_tuple(resp_rv.ip(), resp_rv.port());
    int remote_term = resp_rv.term();
    if (term_ < remote_term) {
        trans2F(remote_term);
    } else if (term_ > remote_term) {
        //do nothing
    } else {
        if (state_ == candidate) {
            candidate_deal_resp_rv_with_same_term(server, resp_rv);
        }
    }
}


void RaftServer::make_resp_rv(bool vote, string context_log, tuple<string, int> server, int lsn) {
    Log_trace << "begin: ";
    Resp_RequestVoteRpc resp_vote;
    resp_vote.set_ok(vote);
    resp_vote.set_term(term_);
    resp_vote.set_lsn(lsn);
    resp_vote.set_ip(ip_);
    resp_vote.set_port(port_);
    string msg;
    resp_vote.SerializeToString(&msg);
    network_.make_rpc_call(RESP_VOTE, server, msg);
}

void RaftServer::make_resp_ae(bool ok, string context_log, tuple<string, int> server, int lsn, bool is_empty_ae) {
    Log_trace << "begin";
    Resp_AppendEntryRpc resp_entry;
    resp_entry.set_ok(ok);
    resp_entry.set_term(term_);
    resp_entry.set_lsn(lsn);
    resp_entry.set_ip(ip_);
    resp_entry.set_port(port_);
    resp_entry.set_is_empty_ae(is_empty_ae);
    string msg;
    resp_entry.SerializeToString(&msg);
    network_.make_rpc_call(RESP_APPEND, server, msg);
}


//helper functions, react2rv and react2ae is very simple in some situations (like term_ < remote), but some situations really wants deal(like for resp2rv a vote is granted), we deal them here
// this is term_ == remote term, otherwise we deal somewhere else(actually just deny or trans2F)
inline void RaftServer::primary_deal_resp_ae_with_same_term(const tuple<string, int> &server, Resp_AppendEntryRpc &resp_ae) {
    Log_trace << "begin";

    int resp_lsn = resp_ae.lsn();
    int current_rpc_lsn = current_rpc_lsn_[server];
    if (current_rpc_lsn != resp_lsn + 1) {
        return;
    } else {
        bool ok = resp_ae.ok();
        int last_send_index = nextIndex_[server];
        Log_debug << "ok: " << ok << ", last_send_index: " << last_send_index << ", entries.size(): " << entries_.size();
        // whether send next entry immediately is determined by primary_deal_resp_ae, but whether to build an empty ae is determinde by AE()

        if (ok) {
            if (last_send_index > entries_.size()) {
                throw_line("last_send_index 不可能大于entry_size");
            }
            if (last_send_index == entries_.size()) {
                //don't nextIndex_[server] = last_send_index + 1;
                Log_debug << "primary has send all index that can be sent, just wait for retry time to expire to send an empty AE(remember check the follower_saved_index)";
            } else {
                /* why we must known the context of rpc (specifically, when received a resp_rpc, we must known the call rpc), why? here is the scene:
                 * Scene 1:
                 *    P and F steady with [0], P haven't send any empty AE as HB, client apply [1], P AE with [1], P received resp_ae,
                 *    ok:1, last_send_index: 1, follower_saved_index: 0, entry size: 2. P should add nextIndex to 2.
                 * Scene 2:
                 *    P and F steady with [0], P send empty AE as HB, client apply [1], P received resp_ae,
                 *    ok:1, last_send_index: 1, follower_saved_index: 0, entry size: 2. It is completely same with scene 1, but P should add nextIndex to 2,
                 *    P should stay nextIndex to 1, and immediately trigger the next AE to send rpc_ae with entry [1].
                 *    That's why we need extra information (the call of AE)
                 *    Let's see braft how to deal with it?
                 */
                if (resp_ae.is_empty_ae()) {  //last AE is empty, a HB
                    Log_debug << "scence 2 happend";
                } else {   //last send is AE with entry
                    nextIndex_[server] = last_send_index + 1;
                    Log_debug << "primary's nextIndex of " << server2str(server) << " is set to " << nextIndex_[server];
                    primary_update_commit_index(server, last_send_index);
                }
                Log_debug << "let's send the next AE immediately";  //两种情况都需要立刻重新发送下一次AE
                retry_timers_[server]->get_core().expires_at(boost::posix_time::min_date_time);
                AE(server);

            }
        } else {
            /* ok == false, but there may be many possibilities, like remote server encouters disk error, what should it do(one way is just crash, no response, so primary will not push back the index to send)
             * so we must ensure !!!!!!!!!!!!! the remote return resp_ae( ok =false ) only in one situation, that prev_index(stored remote) != rpc_index(in AE) -1, any other (disk failure, divide 0) will never return false! just crash it.
             */
            if (last_send_index == 0) {
                throw_line("P sends F a AE with index 0 (maybe with index 0 or emtry), F returns not ok, impossible");
            } else {
                nextIndex_[server] = last_send_index - 1;
                retry_timers_[server]->get_core().cancel();
                AE(server);
            }
        }
    }
}

// candidate deal with resp_rv with the same term (because other resp_rv situation is simple, only this one needs a little logic)
inline void RaftServer::candidate_deal_resp_rv_with_same_term(const tuple<string, int> &server, Resp_RequestVoteRpc &resp_rv) {
    Log_trace << "begin";
    int resp_lsn = resp_rv.lsn();
    int current_rpc_lsn = current_rpc_lsn_[server];
    if (current_rpc_lsn != resp_lsn + 1) {
        Log_debug << "received resp_rv lsn: " << resp_lsn << ", current_lsn: " << current_rpc_lsn << ", not equal, deny!";
        return;
    } else {
        Log_debug << "received resp_rv lsn: " << resp_lsn << ", current_lsn: " << current_rpc_lsn << "equal, the rpc is valid";
        bool ok = resp_rv.ok();
        if (ok) {
            winned_votes_++;
            if (winned_votes_ >= majority_) {
                trans2P();
            } else {
                // cancel the timer, if receive votes, we no longer need to rv more this term
                shared_ptr<Timer> timer = retry_timers_[server];
                timer->get_core().expires_at(boost::posix_time::min_date_time);
            }
        } else {
            // this mean remote follower has already voted for other candidate with the same term, just do nothing and let candidate_timer expires to enter the next term candidate
        }
    }
}


//todo follower modify commit_index and trigger apply_to_state_machine

void RaftServer::apply_to_state_machine(int new_commit_index) {
    /*
     * 希望有另一个线程（或者线程池）来进行state machine 的操作，返回string，再把string回传给io线程，然后io 线程通过shared_ptr写回 resp
     * 需要注意shared_ptr的生命周期， client_modify_str对raft server是不感知的
     */
    smc_.update_commit_index_and_apply(new_commit_index);
}

inline void RaftServer::primary_update_commit_index(const tuple<string, int> &server, int index) {
    Log_trace << "begin: server: " << server2str(server) << ", index: " << index;
    Follower_saved_index[server] = index;
    // this can be done by find the k
    vector<int> temp_sort(configuration_.size(), 0);
    int i = 0;
    for (const auto &ele:Follower_saved_index) {
        Log_debug << server2str(ele.first) << "->" << ele.second;
        temp_sort[i] = ele.second;
        i++;
    }
    std::sort(temp_sort.begin(), temp_sort.end(), greater<int>());
    Log_debug << "sort: " << temp_sort[0] << ", " << temp_sort[1] << ", " << temp_sort[2] << "index : " << majority_ - 1;
    commit_index_ = temp_sort[majority_ - 1];
    Log_debug << "set committed index to " << commit_index_;
    apply_to_state_machine(commit_index_);
}

inline void RaftServer::follower_update_commit_index(int remote_commit_index, int remote_prelog_index) {
    Log_trace << "begin: remote_commit_index: " << remote_commit_index << ", local commit_index: " << commit_index_ << ", remote_prelog_index: " << remote_prelog_index;
    int smaller_index = smaller(remote_commit_index, remote_prelog_index);
    if (smaller_index > commit_index_) {
        commit_index_ = smaller_index;
        Log_debug << "set commit_index to " << smaller_index;
        apply_to_state_machine(commit_index_);
    } else if (smaller_index == commit_index_) {

    } else {
        string err = "remote_commit_index " + std::to_string(smaller_index) + " is smaller than local commit_index " + std::to_string(commit_index_) + ", this is impossible";
        throw_line(err);
    }
}


void RaftServer::write_resp_apply_call(int entry_index, const string res_str) {
    Log_debug << "begin, entry_index: " << entry_index;
    if (state_ == primary) {  //follower_update_index will call a chain to this function finally, then it will try to connection :0, that will raise exception when constructing endpoint
        const auto &remote_addr = entryIndex_to_socketAddr_map_[entry_index];
        network_.make_rpc_call(RESP_CLIENT_APPLY, remote_addr, res_str);
    }
}

void RaftServer::write_resp_query_call(const std::tuple<string, int> &addr, const string res_str) {
    network_.make_rpc_call(RESP_CLIENT_QUERY, addr, res_str);
}

//void RaftServer::deal_with_write_error(boost::system::error_code &ec, std::size_t) {
//    Log_error << " error: " << ec.message();
//}
