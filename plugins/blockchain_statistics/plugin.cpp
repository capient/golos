#include <golos/plugins/blockchain_statistics/plugin.hpp>

#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/chain/history_object.hpp>
#include <golos/chain/index.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/protocol/block.hpp>
// #include <golos/version/hardfork.hpp>
#include <golos/chain/database.hpp>
#include <fc/io/json.hpp>
#include <boost/program_options.hpp>
#include <golos/plugins/blockchain_statistics/statistics_sender.hpp>



namespace golos {
namespace plugins {
namespace blockchain_statistics {

std::string increment_counter(std::string name);
std::string increment_counter(std::string name, uint32_t value);
std::string increment_counter(std::string name, fc::uint128_t value);
std::string increment_counter(std::string name, share_type value);

using namespace golos::protocol;

struct plugin::plugin_impl final {
public:
    plugin_impl( ) : database_(appbase::app().get_plugin<chain::plugin>().db()) {
    }

    ~plugin_impl() = default;

    auto database() -> golos::chain::database & {
        return database_;
    }

    void on_block(const signed_block &b);

    void pre_operation(const operation_notification &o);

    void post_operation(const operation_notification &o);

    golos::chain::database &database_;
    flat_set<uint32_t> _tracked_buckets = {
        60, 3600, 21600, 86400, 604800, 2592000
    };
    flat_set<bucket_id_type> _current_buckets;
    uint32_t _maximum_history_per_bucket_size = 100;

    std::shared_ptr<statistics_sender> stat_sender;
};

struct operation_process {
    const bucket_object &_bucket;
    database &_db;
    std::shared_ptr<statistics_sender> stat_sender;

    operation_process(database &db, const bucket_object &b, std::shared_ptr<statistics_sender> stat_sender) : 
        _bucket(b), _db(db), stat_sender(stat_sender) {
    }

    typedef void result_type;

    template<typename T>
    void operator()(const T &) const {
    }

    void operator()(const transfer_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.transfers++;

            if (op.amount.symbol == STEEM_SYMBOL) {
                b.steem_transferred += op.amount.amount;

                std::string tmp_s = increment_counter("steem_transferred", op.amount.amount);
                stat_sender->push(tmp_s);
            } else {
                b.sbd_transferred += op.amount.amount;

                std::string tmp_s = increment_counter("sbd_transferred", op.amount.amount);
                stat_sender->push(tmp_s);
            }
        });
    }

    void operator()(const interest_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.sbd_paid_as_interest += op.interest.amount;

            std::string tmp_s = increment_counter("sbd_paid_as_interest", op.interest.amount);
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const account_create_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.paid_accounts_created++;

            std::string tmp_s = increment_counter("paid_accounts_created");
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const pow_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            auto &worker = _db.get_account(op.worker_account);

            if (worker.created == _db.head_block_time()) {
                b.mined_accounts_created++;

                std::string tmp_s = increment_counter("mined_accounts_created");
                stat_sender->push(tmp_s);
            }

            b.total_pow++;

            std::string tmp_s = increment_counter("total_pow");
            stat_sender->push(tmp_s);

            uint64_t bits =
                    (_db.get_dynamic_global_properties().num_pow_witnesses /
                     4) + 4;
            uint128_t estimated_hashes = (1 << bits);
            uint32_t delta_t;

            if (b.seconds == 0) {
                delta_t = _db.head_block_time().sec_since_epoch() -
                          b.open.sec_since_epoch();
            } else {
                delta_t = b.seconds;
            }

            b.estimated_hashpower =
                    (b.estimated_hashpower * delta_t +
                     estimated_hashes) / delta_t;

            tmp_s = increment_counter("estimated_hashpower", b.estimated_hashpower);
            stat_sender->push(tmp_s);        
        });
    }

    void operator()(const comment_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            auto &comment = _db.get_comment(op.author, op.permlink);

            if (comment.created == _db.head_block_time()) {
                if (comment.parent_author.length()) {
                    b.replies++;

                    std::string tmp_s = increment_counter("replies");
                    stat_sender->push(tmp_s);
                } else {
                    b.root_comments++;

                    std::string tmp_s = increment_counter("root_comments");
                    stat_sender->push(tmp_s);
                }
            } else {
                if (comment.parent_author.length()) {
                    b.reply_edits++;

                    std::string tmp_s = increment_counter("reply_edits");
                    stat_sender->push(tmp_s);
                } else {
                    b.root_comment_edits++;

                    std::string tmp_s = increment_counter("root_comment_edits");
                    stat_sender->push(tmp_s);
                }
            }
        });
    }

    void operator()(const vote_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            const auto &cv_idx = _db.get_index<comment_vote_index>().indices().get<by_comment_voter>();
            auto &comment = _db.get_comment(op.author, op.permlink);
            auto &voter = _db.get_account(op.voter);
            auto itr = cv_idx.find(boost::make_tuple(comment.id, voter.id));

            if (itr->num_changes) {
                if (comment.parent_author.size()) {
                    b.new_reply_votes++;

                    std::string tmp_s = increment_counter("new_reply_votes");
                    stat_sender->push(tmp_s);
                } else {
                    b.new_root_votes++;

                    std::string tmp_s = increment_counter("new_root_votes");
                    stat_sender->push(tmp_s);
                }
            } else {
                if (comment.parent_author.size()) {
                    b.changed_reply_votes++;

                    std::string tmp_s = increment_counter("changed_reply_votes");
                    stat_sender->push(tmp_s);
                } else {
                    b.changed_root_votes++;

                    std::string tmp_s = increment_counter("changed_root_votes");
                    stat_sender->push(tmp_s);
                }
            }
        });
    }

    void operator()(const author_reward_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.payouts++;
            b.sbd_paid_to_authors += op.sbd_payout.amount;
            b.vests_paid_to_authors += op.vesting_payout.amount;

            std::string tmp_s = increment_counter("payouts");
            stat_sender->push(tmp_s);

            tmp_s = increment_counter("sbd_paid_to_authors", op.sbd_payout.amount);
            stat_sender->push(tmp_s);

            tmp_s = increment_counter("vests_paid_to_authors", op.vesting_payout.amount);
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const curation_reward_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.vests_paid_to_curators += op.reward.amount;

            std::string tmp_s = increment_counter("vests_paid_to_curators", op.reward.amount);
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const liquidity_reward_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.liquidity_rewards_paid += op.payout.amount;

            std::string tmp_s = increment_counter("liquidity_rewards_paid", op.payout.amount);
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const transfer_to_vesting_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.transfers_to_vesting++;
            b.steem_vested += op.amount.amount;

            std::string tmp_s = increment_counter("transfers_to_vesting");
            stat_sender->push(tmp_s);

            tmp_s = increment_counter("steem_vested", op.amount.amount);
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const fill_vesting_withdraw_operation &op) const {
        auto &account = _db.get_account(op.from_account);

        _db.modify(_bucket, [&](bucket_object &b) {
            b.vesting_withdrawals_processed++;

            std::string tmp_s = increment_counter("vesting_withdrawals_processed");
            stat_sender->push(tmp_s);

            if (op.deposited.symbol == STEEM_SYMBOL) {
                b.vests_withdrawn += op.withdrawn.amount;

                tmp_s = increment_counter("vests_withdrawn", op.withdrawn.amount);
                stat_sender->push(tmp_s);
            } else {
                b.vests_transferred += op.withdrawn.amount;

                tmp_s = increment_counter("vests_transferred", op.withdrawn.amount);
                stat_sender->push(tmp_s);
            }

            if (account.vesting_withdraw_rate.amount == 0) {
                b.finished_vesting_withdrawals++;

                tmp_s = increment_counter("finished_vesting_withdrawals");
                stat_sender->push(tmp_s);
            }
        });
    }

    void operator()(const limit_order_create_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.limit_orders_created++;

            std::string tmp_s = increment_counter("limit_orders_created");
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const fill_order_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.limit_orders_filled += 2;

            std::string tmp_s = increment_counter("limit_orders_filled", boost::numeric_cast<uint32_t>(2));
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const limit_order_cancel_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.limit_orders_cancelled++;

            std::string tmp_s = increment_counter("limit_orders_cancelled");
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const convert_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.sbd_conversion_requests_created++;
            b.sbd_to_be_converted += op.amount.amount;

            std::string tmp_s = increment_counter("sbd_conversion_requests_created");
            stat_sender->push(tmp_s);

            tmp_s = increment_counter("sbd_to_be_converted", op.amount.amount);
            stat_sender->push(tmp_s);
        });
    }

    void operator()(const fill_convert_request_operation &op) const {
        _db.modify(_bucket, [&](bucket_object &b) {
            b.sbd_conversion_requests_filled++;
            b.steem_converted += op.amount_out.amount;

            std::string tmp_s = increment_counter("sbd_conversion_requests_filled");
            stat_sender->push(tmp_s);

            tmp_s = increment_counter("steem_converted", op.amount_out.amount);
            stat_sender->push(tmp_s);
        });
    }
};

void plugin::plugin_impl::on_block(const signed_block &b) {
    auto &db = database();

    if (b.block_num() == 1) {
        db.create<bucket_object>([&](bucket_object &bo) {
            bo.open = b.timestamp;
            bo.seconds = 0;
            bo.blocks = 1;
        });
    } else {
        db.modify(db.get(bucket_id_type()), [&](bucket_object &bo) {
            bo.blocks++;
        });
    }

    _current_buckets.clear();
    _current_buckets.insert(bucket_id_type());

    const auto &bucket_idx = db.get_index<bucket_index>().indices().get<by_bucket>();

    uint32_t trx_size = 0;
    uint32_t num_trx = b.transactions.size();

    for (auto trx : b.transactions) {
        trx_size += fc::raw::pack_size(trx);
    }


    for (auto bucket : _tracked_buckets) {
        auto open = fc::time_point_sec(
                (db.head_block_time().sec_since_epoch() / bucket) *
                bucket);
        auto itr = bucket_idx.find(boost::make_tuple(bucket, open));

        if (itr == bucket_idx.end()) {
            _current_buckets.insert(
                    db.create<bucket_object>([&](bucket_object &bo) {
                        bo.open = open;
                        bo.seconds = bucket;
                        bo.blocks = 1;
                    }).id);

            if (_maximum_history_per_bucket_size > 0) {
                try {
                    auto cutoff = fc::time_point_sec((
                            safe<uint32_t>(db.head_block_time().sec_since_epoch()) -
                            safe<uint32_t>(bucket) *
                            safe<uint32_t>(_maximum_history_per_bucket_size)).value);

                    itr = bucket_idx.lower_bound(boost::make_tuple(bucket, fc::time_point_sec()));

                    while (itr->seconds == bucket &&
                           itr->open < cutoff) {
                        auto old_itr = itr;
                        ++itr;
                        db.remove(*old_itr);
                    }
                }
                catch (fc::overflow_exception &e) {
                }
                catch (fc::underflow_exception &e) {
                }
            }
        } else {
            db.modify(*itr, [&](bucket_object &bo) {
                bo.blocks++;
            });

            _current_buckets.insert(itr->id);
        }

        db.modify(*itr, [&](bucket_object &bo) {
            bo.transactions += num_trx;
            bo.bandwidth += trx_size;
        });
    }
}

void plugin::plugin_impl::pre_operation(const operation_notification &o) {
    auto &db = database();

    for (auto bucket_id : _current_buckets) {
        if (o.op.which() ==
            operation::tag<delete_comment_operation>::value) {
            delete_comment_operation op = o.op.get<delete_comment_operation>();
            auto comment = db.get_comment(op.author, op.permlink);
            const auto &bucket = db.get(bucket_id);

            db.modify(bucket, [&](bucket_object &b) {
                if (comment.parent_author.length()) {
                    b.replies_deleted++;
                } else {
                    b.root_comments_deleted++;
                }
            });
        } else if (o.op.which() ==
                   operation::tag<withdraw_vesting_operation>::value) {
            withdraw_vesting_operation op = o.op.get<withdraw_vesting_operation>();
            auto &account = db.get_account(op.account);
            const auto &bucket = db.get(bucket_id);

            auto new_vesting_withdrawal_rate =
                    op.vesting_shares.amount /
                    STEEMIT_VESTING_WITHDRAW_INTERVALS;
            if (op.vesting_shares.amount > 0 &&
                new_vesting_withdrawal_rate == 0) {
                    new_vesting_withdrawal_rate = 1;
            }

            if (!db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                new_vesting_withdrawal_rate *= 10000;
            }

            db.modify(bucket, [&](bucket_object &b) {
                if (account.vesting_withdraw_rate.amount > 0) {
                    b.modified_vesting_withdrawal_requests++;
                } else {
                    b.new_vesting_withdrawal_requests++;
                }

                // TODO: Figure out how to change delta when a vesting withdraw finishes. Have until March 24th 2018 to figure that out...
                b.vesting_withdraw_rate_delta +=
                        new_vesting_withdrawal_rate -
                        account.vesting_withdraw_rate.amount;
            });
        }
    }
}

void plugin::plugin_impl::post_operation(const operation_notification &o) {
    try {
        auto &db = database();

        for (auto bucket_id : _current_buckets) {
            const auto &bucket = db.get(bucket_id);

            if (!is_virtual_operation(o.op)) {
                db.modify(bucket, [&](bucket_object &b) {
                    b.operations++;
                });
            }
            o.op.visit(operation_process(database(), bucket, stat_sender));
        }
    } FC_CAPTURE_AND_RETHROW()
}

plugin::plugin() {

}

plugin::~plugin() {
    _my->stat_sender.reset();//TODO: move to plugin_shutdown
}

void plugin::set_program_options( options_description& cli, options_description& cfg ) {
    cli.add_options()
            ("chain-stats-bucket-size", boost::program_options::value<string>()->default_value("[60,3600,21600,86400,604800,2592000]"),
                    "Track blockchain statistics by grouping orders into buckets of equal size measured in seconds specified as a JSON array of numbers")
            ("chain-stats-history-per-bucket", boost::program_options::value<uint32_t>()->default_value(100),
                    "How far back in time to track history for each bucket size, measured in the number of buckets (default: 100)")
            ("statsd-endpoints", boost::program_options::value<std::vector<std::string>>()->multitoken()->
                    zero_tokens()->composing(), "StatsD endpoints that will receive the statistics in StatsD string format.")
            ("statsd-default-port", boost::program_options::value<uint32_t>()->default_value(8125), "Default port for StatsD nodes.");
    cfg.add(cli);
}

void plugin::plugin_initialize(const boost::program_options::variables_map &options) {
    try {
        ilog("chain_stats_plugin: plugin_initialize() begin");

        _my.reset(new plugin_impl());
        auto &db = _my -> database();

        uint32_t statsd_default_port = 8125; //See default port statsd https://github.com/etsy/statsd

        if (options.count("statsd-default-port")) {
            statsd_default_port = options["statsd-default-port"].as<uint32_t>();
        } // If it's not configured, then we've got some troubles...
        _my->stat_sender = std::shared_ptr<statistics_sender>(new statistics_sender(statsd_default_port) );

        db.applied_block.connect([&](const signed_block &b) {
            _my->on_block(b);
        });
        
        db.pre_apply_operation.connect([&](const operation_notification &o) {
            _my->pre_operation(o);
        });

        db.post_apply_operation.connect([&](const operation_notification &o) {
            _my->post_operation(o);
        });

        add_plugin_index<bucket_index>(db);

        if (options.count("chain-stats-bucket-size")) {
            const std::string &buckets = options["chain-stats-bucket-size"].as<string>();
            _my->_tracked_buckets = fc::json::from_string(buckets).as<flat_set<uint32_t>>();
        }
        if (options.count("chain-stats-history-per-bucket")) {
            _my->_maximum_history_per_bucket_size = options["chain-stats-history-per-bucket"].as<uint32_t>();
        }
        if (options.count("statsd-endpoints")) {
            for (auto it : options["statsd-endpoints"].as<std::vector<std::string>>()) {
                _my->stat_sender->add_address(it);
            }
        }

        wlog("chain-stats-bucket-size: ${b}", ("b", _my->_tracked_buckets));
        wlog("chain-stats-history-per-bucket: ${h}", ("h", _my->_maximum_history_per_bucket_size));

        ilog("chain_stats_plugin: plugin_initialize() end");
    } FC_CAPTURE_AND_RETHROW()
}

void plugin::plugin_startup() {
    ilog("chain_stats plugin: plugin_startup() begin");

    if (_my->stat_sender->can_start()) {
        wlog("chain_stats plugin: statitistics sender was started");
        wlog("StatsD endpoints: ${endpoints}", ( "endpoints", _my->stat_sender->get_endpoint_string_vector() ) );
    }
    else {
        wlog("chain_stats plugin: statitistics sender was not started: no recipient's IPs were provided");
    }

    ilog("chain_stats plugin: plugin_startup() end");
}

const flat_set<uint32_t> &plugin::get_tracked_buckets() const {
    return _my->_tracked_buckets;
}

uint32_t plugin::get_max_history_per_bucket() const {
    return _my->_maximum_history_per_bucket_size;
}

    void plugin::plugin_shutdown() {
    }


    std::string increment_counter(std::string name) {
    std::string res = name + ":1|c";
    return res;
}

std::string increment_counter(std::string name, uint32_t value) {
    std::string res = name + ":";
    std::string num = std::to_string(value);
    res += num + "|c";

    return res;
}
std::string increment_counter(std::string name, fc::uint128_t value) {
    std::string res = name + ":";
    std::string num = std::string(value);
    res += num + "|c";

    return res;
}
std::string increment_counter(std::string name, share_type value) {
    std::string res = name + ":";
    std::string num = std::string(value);
    if (value < 0) {
        res += "-";    
    }
    res += num + "|c";
    return res;
}
} } } // golos:plugin_impl:plugins::blockchain_statistics
