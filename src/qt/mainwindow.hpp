#pragma once

#include "net/runtime.hpp"

#include <QElapsedTimer>
#include <QMainWindow>

#include <cstdint>
#include <optional>

class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QStackedWidget;
class QTableWidget;
class QTimer;

namespace quintum::qtui {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(
        net::NetworkRuntime& runtime,
        const consensus::ChainParams& params,
        QWidget* parent = nullptr
    );

    ~MainWindow() override;

private:
    QWidget* build_overview_page();
    QWidget* build_send_page();
    QWidget* build_receive_page();
    QWidget* build_transactions_page();
    QWidget* build_address_book_page();
    QWidget* build_mining_page();
    QWidget* build_settings_page();

    void refresh();
    void preview_and_send();
    void new_receive_address();
    void copy_receive_address();

    void save_address_book_entry();
    void delete_address_book_entry();

    void toggle_mining();
    void mine_once();

    void show_recovery_phrase();
    void backup_wallet_bundle();

    void apply_snapshot(
        const net::WalletDesktopSnapshot& snapshot
    );

    [[nodiscard]] static QString format_amount(
        Amount value
    );

    [[nodiscard]] static std::optional<Amount>
    parse_amount(const QString& value);

    [[nodiscard]] static QString hash_hex(
        const Hash256& hash
    );

    [[nodiscard]] static QString fee_quote_error_text(
        const wallet::WalletFeeQuote& quote
    );

    [[nodiscard]] static QString mining_error_text(
        NodeMineError error
    );

    net::NetworkRuntime& runtime_;
    const consensus::ChainParams& params_;

    QStackedWidget* pages_{nullptr};

    QLabel* available_value_{nullptr};
    QLabel* confirmed_value_{nullptr};
    QLabel* pending_value_{nullptr};
    QLabel* immature_value_{nullptr};
    QLabel* overview_height_{nullptr};
    QLabel* overview_peer_height_{nullptr};
    QLabel* overview_peers_{nullptr};
    QLabel* overview_mempool_{nullptr};
    QLabel* overview_sync_state_{nullptr};
    QProgressBar* overview_sync_progress_{nullptr};

    QLineEdit* send_address_{nullptr};
    QLineEdit* send_amount_{nullptr};
    QLineEdit* send_label_{nullptr};
    QPushButton* send_button_{nullptr};

    QLineEdit* receive_address_{nullptr};

    QTableWidget* transactions_{nullptr};

    QTableWidget* address_book_table_{nullptr};
    QLineEdit* address_book_address_{nullptr};
    QLineEdit* address_book_label_{nullptr};

    QLabel* mining_state_{nullptr};
    QLabel* mining_hashrate_{nullptr};
    QLabel* mining_attempts_{nullptr};
    QLabel* mining_blocks_{nullptr};
    QPushButton* mining_button_{nullptr};
    QTimer* mining_timer_{nullptr};
    QElapsedTimer mining_elapsed_{};
    std::uint64_t mining_total_attempts_{0U};
    std::uint64_t mining_blocks_found_{0U};

    QLabel* settings_network_{nullptr};
    QLabel* settings_p2p_port_{nullptr};
    QLabel* settings_listen_port_{nullptr};
    QLabel* settings_fee_rate_{nullptr};

    QLabel* status_network_{nullptr};
    QLabel* status_blocks_{nullptr};
    QLabel* status_peers_{nullptr};
    QLabel* status_connection_{nullptr};

    QTimer* refresh_timer_{nullptr};
};

} // namespace quintum::qtui
