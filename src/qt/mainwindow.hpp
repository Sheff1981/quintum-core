#pragma once

#include "net/runtime.hpp"

#include <QMainWindow>

#include <memory>
#include <optional>

class QLabel;
class QLineEdit;
class QPushButton;
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

private:
    QWidget* build_overview_page();
    QWidget* build_send_page();
    QWidget* build_receive_page();
    QWidget* build_transactions_page();

    void refresh();
    void preview_and_send();
    void new_receive_address();
    void copy_receive_address();

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

    net::NetworkRuntime& runtime_;
    const consensus::ChainParams& params_;

    QStackedWidget* pages_{nullptr};

    QLabel* available_value_{nullptr};
    QLabel* confirmed_value_{nullptr};
    QLabel* pending_value_{nullptr};
    QLabel* immature_value_{nullptr};
    QLabel* overview_height_{nullptr};
    QLabel* overview_peers_{nullptr};
    QLabel* overview_mempool_{nullptr};

    QLineEdit* send_address_{nullptr};
    QLineEdit* send_amount_{nullptr};
    QLineEdit* send_label_{nullptr};
    QPushButton* send_button_{nullptr};

    QLineEdit* receive_address_{nullptr};

    QTableWidget* transactions_{nullptr};

    QLabel* status_network_{nullptr};
    QLabel* status_blocks_{nullptr};
    QLabel* status_peers_{nullptr};
    QLabel* status_connection_{nullptr};

    QTimer* refresh_timer_{nullptr};
};

} // namespace quintum::qtui
