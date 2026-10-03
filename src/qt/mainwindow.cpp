#include "qt/mainwindow.hpp"

#include "consensus/monetary.hpp"
#include "crypto/random.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <string>

namespace quintum::qtui {
namespace {

QLabel* make_amount_label()
{
    auto* label = new QLabel("0.00000000 QTM");
    QFont font = label->font();
    font.setPointSize(font.pointSize() + 5);
    font.setBold(true);
    label->setFont(font);
    return label;
}

QFrame* make_balance_card(
    const QString& title,
    QLabel*& value)
{
    auto* frame = new QFrame;
    frame->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(frame);
    auto* caption = new QLabel(title);
    value = make_amount_label();

    layout->addWidget(caption);
    layout->addWidget(value);
    layout->addStretch();

    return frame;
}

QString status_text(
    wallet::WalletTransactionStatus status)
{
    switch (status) {
    case wallet::WalletTransactionStatus::unconfirmed:
        return "Pending";
    case wallet::WalletTransactionStatus::confirmed:
        return "Confirmed";
    case wallet::WalletTransactionStatus::inactive:
        return "Inactive";
    }

    return "Unknown";
}

QString metadata_error_text(
    wallet::WalletMetadataError error)
{
    using wallet::WalletMetadataError;

    switch (error) {
    case WalletMetadataError::none:
        return {};
    case WalletMetadataError::io_error:
        return "Wallet metadata could not be written safely.";
    case WalletMetadataError::corrupt:
        return "Wallet metadata is corrupt.";
    case WalletMetadataError::wrong_network:
        return "This metadata belongs to another QUINTUM network.";
    case WalletMetadataError::wrong_wallet:
        return "This metadata belongs to another wallet.";
    case WalletMetadataError::invalid_address:
        return "The address is not a valid QUINTUM address.";
    case WalletMetadataError::wrong_network_address:
        return "The address belongs to another QUINTUM network.";
    case WalletMetadataError::invalid_label:
        return "The label is empty, too long, or contains unsupported control characters.";
    case WalletMetadataError::too_many_entries:
        return "The address book has reached its safety limit.";
    }

    return "Wallet metadata operation failed.";
}

std::filesystem::path filesystem_path(
    const QString& value)
{
#ifdef _WIN32
    return std::filesystem::path{
        value.toStdWString()
    };
#else
    const QByteArray utf8 =
        value.toUtf8();

    return std::filesystem::path{
        std::string{
            utf8.constData(),
            static_cast<std::size_t>(
                utf8.size()
            )
        }
    };
#endif
}

} // namespace

MainWindow::MainWindow(
    net::NetworkRuntime& runtime,
    const consensus::ChainParams& params,
    QWidget* parent)
    : QMainWindow(parent),
      runtime_(runtime),
      params_(params)
{
    switch (params_.network) {
    case consensus::Network::testnet:
        setWindowTitle("QUINTUM Core [testnet]");
        break;
    case consensus::Network::regtest:
        setWindowTitle("QUINTUM Core [regtest]");
        break;
    case consensus::Network::mainnet:
        setWindowTitle("QUINTUM Core");
        break;
    }

    resize(1060, 690);

    pages_ = new QStackedWidget;
    pages_->addWidget(build_overview_page());
    pages_->addWidget(build_send_page());
    pages_->addWidget(build_receive_page());
    pages_->addWidget(build_transactions_page());
    pages_->addWidget(build_address_book_page());
    pages_->addWidget(build_mining_page());
    pages_->addWidget(build_settings_page());
    setCentralWidget(pages_);

    auto* overview_action =
        new QAction("Overview", this);
    auto* send_action =
        new QAction("Send", this);
    auto* receive_action =
        new QAction("Receive", this);
    auto* transactions_action =
        new QAction("Transactions", this);
    auto* mining_action =
        new QAction("Mining", this);

    auto* navigation =
        addToolBar("Wallet navigation");
    navigation->setMovable(false);
    navigation->setFloatable(false);
    navigation->addAction(overview_action);
    navigation->addAction(send_action);
    navigation->addAction(receive_action);
    navigation->addAction(transactions_action);
    navigation->addSeparator();
    navigation->addAction(mining_action);

    connect(
        overview_action,
        &QAction::triggered,
        this,
        [this] { pages_->setCurrentIndex(0); }
    );
    connect(
        send_action,
        &QAction::triggered,
        this,
        [this] { pages_->setCurrentIndex(1); }
    );
    connect(
        receive_action,
        &QAction::triggered,
        this,
        [this] { pages_->setCurrentIndex(2); }
    );
    connect(
        transactions_action,
        &QAction::triggered,
        this,
        [this] { pages_->setCurrentIndex(3); }
    );
    connect(
        mining_action,
        &QAction::triggered,
        this,
        [this] { pages_->setCurrentIndex(5); }
    );

    auto* file_menu =
        menuBar()->addMenu("&File");
    auto* backup_action =
        file_menu->addAction("Backup Wallet...");
    auto* recovery_action =
        file_menu->addAction("Show Recovery Words...");
    file_menu->addSeparator();
    auto* exit_action =
        file_menu->addAction("E&xit");

    connect(
        backup_action,
        &QAction::triggered,
        this,
        [this] { backup_wallet_bundle(); }
    );
    connect(
        recovery_action,
        &QAction::triggered,
        this,
        [this] { show_recovery_phrase(); }
    );
    connect(
        exit_action,
        &QAction::triggered,
        this,
        &QWidget::close
    );

    auto* settings_menu =
        menuBar()->addMenu("&Settings");
    auto* options_action =
        settings_menu->addAction("Options...");
    connect(
        options_action,
        &QAction::triggered,
        this,
        [this] { pages_->setCurrentIndex(6); }
    );

    auto* window_menu =
        menuBar()->addMenu("&Window");
    auto* minimize_action =
        window_menu->addAction("Minimize");
    minimize_action->setShortcut(
        QKeySequence("Ctrl+M")
    );
    connect(
        minimize_action,
        &QAction::triggered,
        this,
        &QWidget::showMinimized
    );

    window_menu->addSeparator();

    auto* sending_addresses =
        window_menu->addAction("Sending addresses");
    auto* receiving_addresses =
        window_menu->addAction("Receiving addresses");

    connect(
        sending_addresses,
        &QAction::triggered,
        this,
        [this] { pages_->setCurrentIndex(4); }
    );
    connect(
        receiving_addresses,
        &QAction::triggered,
        this,
        [this] { pages_->setCurrentIndex(2); }
    );

    window_menu->addSeparator();

    auto* information_action =
        window_menu->addAction("Information");
    information_action->setShortcut(
        QKeySequence("Ctrl+I")
    );
    auto* console_action =
        window_menu->addAction("Console");
    console_action->setShortcut(
        QKeySequence("Ctrl+T")
    );
    auto* traffic_action =
        window_menu->addAction("Network Traffic");
    traffic_action->setShortcut(
        QKeySequence("Ctrl+N")
    );
    auto* peers_action =
        window_menu->addAction("Peers");
    peers_action->setShortcut(
        QKeySequence("Ctrl+P")
    );

    connect(
        information_action,
        &QAction::triggered,
        this,
        [this] { show_debug_window(0); }
    );
    connect(
        console_action,
        &QAction::triggered,
        this,
        [this] { show_debug_window(1); }
    );
    connect(
        traffic_action,
        &QAction::triggered,
        this,
        [this] { show_debug_window(2); }
    );
    connect(
        peers_action,
        &QAction::triggered,
        this,
        [this] { show_debug_window(3); }
    );

    auto* help_menu =
        menuBar()->addMenu("&Help");
    auto* about_action =
        help_menu->addAction("About QUINTUM Core");
    connect(
        about_action,
        &QAction::triggered,
        this,
        [this] { show_about(); }
    );

    status_network_ = new QLabel;
    status_blocks_ = new QLabel;
    status_peers_ = new QLabel;
    status_connection_ = new QLabel;

    statusBar()->addPermanentWidget(status_network_);
    statusBar()->addPermanentWidget(status_blocks_);
    statusBar()->addPermanentWidget(status_peers_);
    statusBar()->addPermanentWidget(status_connection_);

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(1000);

    connect(
        refresh_timer_,
        &QTimer::timeout,
        this,
        [this] {
            refresh();
        }
    );

    mining_timer_ = new QTimer(this);
    mining_timer_->setInterval(100);

    connect(
        mining_timer_,
        &QTimer::timeout,
        this,
        [this] {
            mine_once();
        }
    );

    refresh_timer_->start();
    refresh();
}

MainWindow::~MainWindow()
{
    if (mining_timer_ != nullptr) {
        mining_timer_->stop();
    }
}

QWidget* MainWindow::build_overview_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("Overview");
    QFont title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 7);
    title_font.setBold(true);
    title->setFont(title_font);

    auto* balances = new QHBoxLayout;
    balances->addWidget(
        make_balance_card(
            "Available",
            available_value_
        )
    );
    balances->addWidget(
        make_balance_card(
            "Confirmed",
            confirmed_value_
        )
    );
    balances->addWidget(
        make_balance_card(
            "Pending",
            pending_value_
        )
    );
    balances->addWidget(
        make_balance_card(
            "Immature",
            immature_value_
        )
    );

    auto* network_frame = new QFrame;
    network_frame->setFrameShape(QFrame::StyledPanel);
    auto* network_layout = new QFormLayout(network_frame);

    overview_height_ = new QLabel("-");
    overview_peer_height_ = new QLabel("-");
    overview_peers_ = new QLabel("0");
    overview_mempool_ = new QLabel("0");
    overview_sync_state_ = new QLabel("Starting");
    overview_sync_progress_ = new QProgressBar;
    overview_sync_progress_->setRange(0, 100);
    overview_sync_progress_->setValue(0);

    network_layout->addRow("Local block height:", overview_height_);
    network_layout->addRow("Peer best height:", overview_peer_height_);
    network_layout->addRow("Peers:", overview_peers_);
    network_layout->addRow("Mempool:", overview_mempool_);
    network_layout->addRow("Synchronization:", overview_sync_state_);
    network_layout->addRow("Progress:", overview_sync_progress_);

    layout->addWidget(title);
    layout->addLayout(balances);
    layout->addWidget(network_frame);
    layout->addStretch();

    return page;
}

QWidget* MainWindow::build_send_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("Send QUINTUM");
    QFont title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 7);
    title_font.setBold(true);
    title->setFont(title_font);

    auto* form = new QFormLayout;

    send_address_ = new QLineEdit;
    send_address_->setPlaceholderText("QUINTUM address");

    send_amount_ = new QLineEdit;
    send_amount_->setPlaceholderText("0.00000000");

    send_label_ = new QLineEdit;
    send_label_->setPlaceholderText("Optional recipient label");

    form->addRow("Pay to:", send_address_);
    form->addRow("Amount (QTM):", send_amount_);
    form->addRow("Label:", send_label_);

    send_button_ = new QPushButton("Preview and send");

    connect(
        send_button_,
        &QPushButton::clicked,
        this,
        [this] {
            preview_and_send();
        }
    );

    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(send_button_);
    layout->addStretch();

    return page;
}

QWidget* MainWindow::build_receive_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("Receive QUINTUM");
    QFont title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 7);
    title_font.setBold(true);
    title->setFont(title_font);

    receive_address_ = new QLineEdit;
    receive_address_->setReadOnly(true);

    auto* buttons = new QHBoxLayout;
    auto* copy = new QPushButton("Copy address");
    auto* fresh = new QPushButton("New address");

    connect(
        copy,
        &QPushButton::clicked,
        this,
        [this] {
            copy_receive_address();
        }
    );

    connect(
        fresh,
        &QPushButton::clicked,
        this,
        [this] {
            new_receive_address();
        }
    );

    buttons->addWidget(copy);
    buttons->addWidget(fresh);
    buttons->addStretch();

    layout->addWidget(title);
    layout->addWidget(new QLabel(
        "Current receive address:"
    ));
    layout->addWidget(receive_address_);
    layout->addLayout(buttons);
    layout->addStretch();

    return page;
}

QWidget* MainWindow::build_transactions_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("Transactions");
    QFont title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 7);
    title_font.setBold(true);
    title->setFont(title_font);

    transactions_ = new QTableWidget;
    transactions_->setColumnCount(7);
    transactions_->setHorizontalHeaderLabels({
        "Status",
        "TxID",
        "Received",
        "Spent",
        "Fee",
        "Confirmations",
        "Label",
    });
    transactions_->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );
    transactions_->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );
    transactions_->horizontalHeader()
        ->setStretchLastSection(true);
    transactions_->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::ResizeToContents
        );

    layout->addWidget(title);
    layout->addWidget(transactions_, 1);

    return page;
}

QWidget* MainWindow::build_address_book_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("Address Book");
    QFont title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 7);
    title_font.setBold(true);
    title->setFont(title_font);

    address_book_table_ = new QTableWidget;
    address_book_table_->setColumnCount(2);
    address_book_table_->setHorizontalHeaderLabels({
        "Address",
        "Label",
    });
    address_book_table_->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );
    address_book_table_->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );
    address_book_table_->horizontalHeader()
        ->setStretchLastSection(true);

    auto* form = new QFormLayout;
    address_book_address_ = new QLineEdit;
    address_book_label_ = new QLineEdit;

    form->addRow("Address:", address_book_address_);
    form->addRow("Label:", address_book_label_);

    auto* buttons = new QHBoxLayout;
    auto* save = new QPushButton("Save");
    auto* remove = new QPushButton("Delete");

    buttons->addWidget(save);
    buttons->addWidget(remove);
    buttons->addStretch();

    connect(
        save,
        &QPushButton::clicked,
        this,
        [this] {
            save_address_book_entry();
        }
    );

    connect(
        remove,
        &QPushButton::clicked,
        this,
        [this] {
            delete_address_book_entry();
        }
    );

    connect(
        address_book_table_,
        &QTableWidget::itemSelectionChanged,
        this,
        [this] {
            const int row =
                address_book_table_
                    ->currentRow();

            if (row < 0) {
                return;
            }

            const auto* address =
                address_book_table_->item(
                    row,
                    0
                );
            const auto* label =
                address_book_table_->item(
                    row,
                    1
                );

            if (address != nullptr) {
                address_book_address_->setText(
                    address->text()
                );
            }

            if (label != nullptr) {
                address_book_label_->setText(
                    label->text()
                );
            }
        }
    );

    layout->addWidget(title);
    layout->addWidget(address_book_table_, 1);
    layout->addLayout(form);
    layout->addLayout(buttons);

    return page;
}

QWidget* MainWindow::build_mining_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("Mining");
    QFont title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 7);
    title_font.setBold(true);
    title->setFont(title_font);

    auto* info = new QLabel(
        "Mining performs real QUINTUM Proof of Work and pays valid coinbase rewards to this wallet. "
        "Found blocks are submitted locally and relayed to peers."
    );
    info->setWordWrap(true);

    auto* form = new QFormLayout;
    mining_state_ = new QLabel("Stopped");
    mining_hashrate_ = new QLabel("0 H/s");
    mining_attempts_ = new QLabel("0");
    mining_blocks_ = new QLabel("0");

    form->addRow("Status:", mining_state_);
    form->addRow("Hash rate:", mining_hashrate_);
    form->addRow("Hashes attempted:", mining_attempts_);
    form->addRow("Blocks found:", mining_blocks_);

    mining_button_ = new QPushButton("Start mining");

    connect(
        mining_button_,
        &QPushButton::clicked,
        this,
        [this] {
            toggle_mining();
        }
    );

    layout->addWidget(title);
    layout->addWidget(info);
    layout->addLayout(form);
    layout->addWidget(mining_button_);
    layout->addStretch();

    return page;
}

QWidget* MainWindow::build_settings_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("Settings");
    QFont title_font = title->font();
    title_font.setPointSize(title_font.pointSize() + 7);
    title_font.setBold(true);
    title->setFont(title_font);

    auto* form = new QFormLayout;
    settings_network_ = new QLabel;
    settings_p2p_port_ = new QLabel;
    settings_listen_port_ = new QLabel;
    settings_fee_rate_ = new QLabel;

    form->addRow("Network:", settings_network_);
    form->addRow("Default P2P port:", settings_p2p_port_);
    form->addRow("Listening port:", settings_listen_port_);
    form->addRow("Recommended fee rate:", settings_fee_rate_);

    auto* recovery = new QPushButton(
        "Show 24 recovery words"
    );
    auto* backup = new QPushButton(
        "Backup complete wallet"
    );

    auto* backup_note = new QLabel(
        "Creates one .qtmbackup file containing wallet.dat and encrypted wallet metadata. Blockchain data is not included because it can be synchronized again."
    );
    backup_note->setWordWrap(true);

    connect(
        recovery,
        &QPushButton::clicked,
        this,
        [this] {
            show_recovery_phrase();
        }
    );

    connect(
        backup,
        &QPushButton::clicked,
        this,
        [this] {
            backup_wallet_bundle();
        }
    );

    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(recovery);
    layout->addWidget(backup);
    layout->addWidget(backup_note);
    layout->addStretch();

    return page;
}

void MainWindow::show_debug_window(
    int tab_index)
{
    auto* dialog = new QDialog(this);
    dialog->setAttribute(
        Qt::WA_DeleteOnClose
    );
    dialog->setWindowTitle("Debug window");
    dialog->resize(760, 500);

    auto* layout = new QVBoxLayout(dialog);
    auto* tabs = new QTabWidget(dialog);

    const auto snapshot =
        runtime_.desktop_snapshot();
    const auto& status =
        snapshot.status;

    auto* information = new QWidget;
    auto* information_form =
        new QFormLayout(information);

    const QString network_name =
        QString::fromUtf8(
            params_.name.data(),
            static_cast<qsizetype>(
                params_.name.size()
            )
        );

    information_form->addRow(
        "Client version:",
        new QLabel(
            QCoreApplication::
                applicationVersion()
        )
    );
    information_form->addRow(
        "Network:",
        new QLabel(network_name)
    );
    information_form->addRow(
        "Number of connections:",
        new QLabel(
            QString::number(
                status.peers
            )
        )
    );
    information_form->addRow(
        "Block height:",
        new QLabel(
            status.height
                ? QString::number(
                      *status.height
                  )
                : "-"
        )
    );
    information_form->addRow(
        "Best peer height:",
        new QLabel(
            status.peer_best_height
                ? QString::number(
                      *status.peer_best_height
                  )
                : "-"
        )
    );
    information_form->addRow(
        "Mempool transactions:",
        new QLabel(
            QString::number(
                status.mempool_transactions
            )
        )
    );
    information_form->addRow(
        "P2P listen port:",
        new QLabel(
            QString::number(
                status.listen_port
            )
        )
    );
    information_form->addRow(
        "Best block:",
        new QLabel(
            status.tip
                ? hash_hex(*status.tip)
                : "-"
        )
    );

    auto* console = new QWidget;
    auto* console_layout =
        new QVBoxLayout(console);
    auto* console_text = new QTextEdit;
    console_text->setReadOnly(true);
    console_text->setPlainText(
        "QUINTUM Core debug console\n\n"
        "The desktop RPC command console is not exposed yet. "
        "No fake commands are provided: when RPC is added, "
        "this tab will execute the same validated node/wallet APIs."
    );
    console_layout->addWidget(console_text);

    auto* traffic = new QWidget;
    auto* traffic_form =
        new QFormLayout(traffic);
    traffic_form->addRow(
        "Active connections:",
        new QLabel(
            QString::number(
                status.peers
            )
        )
    );
    traffic_form->addRow(
        "Known peer addresses:",
        new QLabel(
            QString::number(
                status.known_addresses
            )
        )
    );
    traffic_form->addRow(
        "Mempool transactions:",
        new QLabel(
            QString::number(
                status.mempool_transactions
            )
        )
    );
    auto* traffic_note = new QLabel(
        "Per-peer byte counters are not exposed by the current runtime yet."
    );
    traffic_note->setWordWrap(true);
    traffic_form->addRow(
        "Traffic counters:",
        traffic_note
    );

    auto* peers = new QWidget;
    auto* peers_form =
        new QFormLayout(peers);

    const std::size_t inbound =
        status.peers >= status.outbound_peers
            ? status.peers -
                  status.outbound_peers
            : 0U;

    peers_form->addRow(
        "Connections:",
        new QLabel(
            QString::number(
                status.peers
            )
        )
    );
    peers_form->addRow(
        "Outbound:",
        new QLabel(
            QString::number(
                status.outbound_peers
            )
        )
    );
    peers_form->addRow(
        "Inbound:",
        new QLabel(
            QString::number(
                inbound
            )
        )
    );
    peers_form->addRow(
        "Known addresses:",
        new QLabel(
            QString::number(
                status.known_addresses
            )
        )
    );
    peers_form->addRow(
        "Peer best height:",
        new QLabel(
            status.peer_best_height
                ? QString::number(
                      *status.peer_best_height
                  )
                : "-"
        )
    );
    auto* peers_note = new QLabel(
        "The current runtime exposes aggregate peer state. "
        "A per-peer table will be wired to real peer-session snapshots "
        "before the public desktop release."
    );
    peers_note->setWordWrap(true);
    peers_form->addRow(
        "Details:",
        peers_note
    );

    tabs->addTab(
        information,
        "Information"
    );
    tabs->addTab(
        console,
        "Console"
    );
    tabs->addTab(
        traffic,
        "Network Traffic"
    );
    tabs->addTab(
        peers,
        "Peers"
    );

    tabs->setCurrentIndex(
        std::clamp(
            tab_index,
            0,
            tabs->count() - 1
        )
    );

    layout->addWidget(tabs);
    dialog->show();
}

void MainWindow::show_about()
{
    QMessageBox::about(
        this,
        "About QUINTUM Core",
        "QUINTUM Core " +
            QCoreApplication::
                applicationVersion() +
            "\n\nIndependent Proof-of-Work full node and wallet. "
            "The desktop follows proven Bitcoin Core interaction patterns "
            "while using QUINTUM's own network, consensus parameters, "
            "addresses and branding."
    );
}

void MainWindow::refresh()
{
    apply_snapshot(
        runtime_.desktop_snapshot()
    );
}

void MainWindow::apply_snapshot(
    const net::WalletDesktopSnapshot& snapshot)
{
    const auto& status = snapshot.status;
    const auto& balance = status.wallet_balance;

    available_value_->setText(
        format_amount(balance.available) +
        " QTM"
    );
    confirmed_value_->setText(
        format_amount(balance.confirmed) +
        " QTM"
    );
    pending_value_->setText(
        format_amount(balance.pending) +
        " QTM"
    );
    immature_value_->setText(
        format_amount(balance.immature) +
        " QTM"
    );

    const QString height =
        status.height
            ? QString::number(*status.height)
            : "-";

    const QString peer_height =
        status.peer_best_height
            ? QString::number(
                  *status.peer_best_height
              )
            : "-";

    overview_height_->setText(height);
    overview_peer_height_->setText(peer_height);
    overview_peers_->setText(
        QString::number(status.peers)
    );
    overview_mempool_->setText(
        QString::number(
            status.mempool_transactions
        )
    );

    if (status.synchronizing) {
        overview_sync_state_->setText(
            "Synchronizing"
        );
    } else if (status.peer_best_height) {
        overview_sync_state_->setText(
            "Up to date"
        );
    } else if (status.peers == 0U) {
        overview_sync_state_->setText(
            "Waiting for peers"
        );
    } else {
        overview_sync_state_->setText(
            "Connected"
        );
    }

    const int progress =
        status.peer_best_height
            ? std::clamp(
                  static_cast<int>(
                      status.sync_progress *
                      100.0
                  ),
                  0,
                  100
              )
            : 0;

    overview_sync_progress_->setValue(
        progress
    );

    receive_address_->setText(
        QString::fromStdString(
            status.receive_address
        )
    );

    status_network_->setText(
        QString("Network: %1")
            .arg(
                QString::fromUtf8(
                    params_.name.data(),
                    static_cast<qsizetype>(
                        params_.name.size()
                    )
                )
            )
    );
    status_blocks_->setText(
        "Blocks: " + height
    );
    status_peers_->setText(
        QString("Peers: %1")
            .arg(status.peers)
    );
    status_connection_->setText(
        status.running
            ? "Node: running"
            : "Node: stopped"
    );

    settings_network_->setText(
        QString::fromUtf8(
            params_.name.data(),
            static_cast<qsizetype>(
                params_.name.size()
            )
        )
    );
    settings_p2p_port_->setText(
        QString::number(
            params_.p2p_port
        )
    );
    settings_listen_port_->setText(
        QString::number(
            status.listen_port
        )
    );
    settings_fee_rate_->setText(
        QString("%1 atomic / 1000 bytes")
            .arg(
                static_cast<qulonglong>(
                    status.
                        recommended_fee_rate_per_kb
                )
            )
    );

    transactions_->setRowCount(
        static_cast<int>(
            snapshot.transactions.size()
        )
    );

    for (std::size_t i = 0U;
         i < snapshot.transactions.size();
         ++i) {
        const auto& view =
            snapshot.transactions[i];
        const auto& tx = view.record;
        const int row =
            static_cast<int>(i);

        transactions_->setItem(
            row,
            0,
            new QTableWidgetItem(
                status_text(tx.status)
            )
        );
        transactions_->setItem(
            row,
            1,
            new QTableWidgetItem(
                hash_hex(tx.txid)
            )
        );
        transactions_->setItem(
            row,
            2,
            new QTableWidgetItem(
                format_amount(tx.received)
            )
        );
        transactions_->setItem(
            row,
            3,
            new QTableWidgetItem(
                format_amount(tx.spent)
            )
        );
        transactions_->setItem(
            row,
            4,
            new QTableWidgetItem(
                tx.fee
                    ? format_amount(*tx.fee)
                    : "-"
            )
        );
        transactions_->setItem(
            row,
            5,
            new QTableWidgetItem(
                QString::number(
                    tx.confirmations
                )
            )
        );
        transactions_->setItem(
            row,
            6,
            new QTableWidgetItem(
                view.label
                    ? QString::fromStdString(
                          *view.label
                      )
                    : QString{}
            )
        );
    }

    address_book_table_->setRowCount(
        static_cast<int>(
            snapshot.address_book.size()
        )
    );

    for (std::size_t i = 0U;
         i < snapshot.address_book.size();
         ++i) {
        const int row =
            static_cast<int>(i);

        address_book_table_->setItem(
            row,
            0,
            new QTableWidgetItem(
                QString::fromStdString(
                    snapshot.address_book[i].
                        address
                )
            )
        );
        address_book_table_->setItem(
            row,
            1,
            new QTableWidgetItem(
                QString::fromStdString(
                    snapshot.address_book[i].
                        label
                )
            )
        );
    }
}

void MainWindow::preview_and_send()
{
    const QString destination =
        send_address_->text().trimmed();

    const auto amount =
        parse_amount(
            send_amount_->text()
        );

    if (destination.isEmpty() ||
        !amount ||
        *amount == 0U) {
        QMessageBox::warning(
            this,
            "Invalid payment",
            "Enter a valid QUINTUM address and a positive amount."
        );
        return;
    }

    send_button_->setEnabled(false);

    const auto preview =
        runtime_.preview_send(
            destination.toStdString(),
            *amount
        );

    send_button_->setEnabled(true);

    if (!preview.ok()) {
        QMessageBox::warning(
            this,
            "Payment cannot be created",
            fee_quote_error_text(
                preview.quote
            )
        );
        refresh();
        return;
    }

    const Amount total =
        preview.amount + preview.quote.fee;

    QString label =
        send_label_->text().trimmed();

    if (label.isEmpty() &&
        preview.recipient_label) {
        label =
            QString::fromStdString(
                *preview.recipient_label
            );
    }

    QString message;
    message +=
        "Recipient:\n" +
        destination +
        "\n\nAmount: " +
        format_amount(preview.amount) +
        " QTM\nFee: " +
        format_amount(preview.quote.fee) +
        " QTM\nTotal: " +
        format_amount(total) +
        " QTM";

    if (!label.isEmpty()) {
        message +=
            "\nLabel: " + label;
    }

    const auto answer =
        QMessageBox::question(
            this,
            "Confirm payment",
            message,
            QMessageBox::Yes |
                QMessageBox::Cancel,
            QMessageBox::Cancel
        );

    if (answer != QMessageBox::Yes) {
        return;
    }

    const auto sent =
        runtime_.confirm_send(preview);

    if (!sent.ok()) {
        QString error;

        switch (sent.error) {
        case net::NetworkWalletSendError::stale_preview:
            error =
                "The blockchain or mempool changed while you were confirming. Review the payment again.";
            break;
        case net::NetworkWalletSendError::invalid_preview:
            error =
                "The payment preview changed and was rejected before signing.";
            break;
        case net::NetworkWalletSendError::wallet_create_failed:
            error =
                "The wallet could no longer create this transaction. Refresh the payment and try again.";
            break;
        case net::NetworkWalletSendError::node_rejected:
            error =
                "The local node rejected the transaction before relay.";
            break;
        case net::NetworkWalletSendError::wallet_sync_failed:
            error =
                "The transaction was accepted, but the wallet could not refresh its local state.";
            break;
        case net::NetworkWalletSendError::none:
            break;
        }

        QMessageBox::warning(
            this,
            "Payment not sent",
            error
        );
        refresh();
        return;
    }

    if (!label.isEmpty()) {
        const std::string label_utf8 =
            label.toStdString();

        const auto address_saved =
            runtime_.set_address_label(
                preview.destination,
                label_utf8
            );

        const auto tx_saved =
            runtime_.set_transaction_label(
                sent.wallet.txid,
                label_utf8
            );

        if (address_saved !=
                wallet::WalletMetadataError::none ||
            tx_saved !=
                wallet::WalletMetadataError::none) {
            QMessageBox::warning(
                this,
                "Payment sent, label not saved",
                "The transaction was sent successfully, but its local label could not be saved."
            );
        }
    }

    send_address_->clear();
    send_amount_->clear();
    send_label_->clear();

    QMessageBox::information(
        this,
        "Payment sent",
        "Transaction accepted by the local node.\n\nTxID:\n" +
            hash_hex(sent.wallet.txid)
    );

    refresh();
}

void MainWindow::new_receive_address()
{
    const auto created =
        runtime_.new_receive_address();

    if (!created.ok()) {
        QMessageBox::warning(
            this,
            "Address not created",
            "The wallet could not reserve and persist a new receive address."
        );
        return;
    }

    receive_address_->setText(
        QString::fromStdString(
            created.address
        )
    );

    QApplication::clipboard()->setText(
        receive_address_->text()
    );

    refresh();
}

void MainWindow::copy_receive_address()
{
    QApplication::clipboard()->setText(
        receive_address_->text()
    );
}

void MainWindow::save_address_book_entry()
{
    const QString address =
        address_book_address_
            ->text()
            .trimmed();
    const QString label =
        address_book_label_
            ->text()
            .trimmed();

    if (address.isEmpty() ||
        label.isEmpty()) {
        QMessageBox::warning(
            this,
            "Address book",
            "Enter both an address and a label."
        );
        return;
    }

    const auto result =
        runtime_.set_address_label(
            address.toStdString(),
            label.toStdString()
        );

    if (result !=
        wallet::WalletMetadataError::none) {
        QMessageBox::warning(
            this,
            "Address not saved",
            metadata_error_text(result)
        );
        return;
    }

    address_book_address_->clear();
    address_book_label_->clear();
    refresh();
}

void MainWindow::delete_address_book_entry()
{
    const QString address =
        address_book_address_
            ->text()
            .trimmed();

    if (address.isEmpty()) {
        QMessageBox::warning(
            this,
            "Address book",
            "Select or enter an address to delete."
        );
        return;
    }

    const auto answer =
        QMessageBox::question(
            this,
            "Delete address-book entry",
            "Remove the local label for this address?",
            QMessageBox::Yes |
                QMessageBox::Cancel,
            QMessageBox::Cancel
        );

    if (answer != QMessageBox::Yes) {
        return;
    }

    const auto result =
        runtime_.set_address_label(
            address.toStdString(),
            ""
        );

    if (result !=
        wallet::WalletMetadataError::none) {
        QMessageBox::warning(
            this,
            "Address not deleted",
            metadata_error_text(result)
        );
        return;
    }

    address_book_address_->clear();
    address_book_label_->clear();
    refresh();
}

void MainWindow::toggle_mining()
{
    if (mining_timer_->isActive()) {
        mining_timer_->stop();
        mining_state_->setText("Stopped");
        mining_button_->setText("Start mining");
        return;
    }

    mining_total_attempts_ = 0U;
    mining_blocks_found_ = 0U;
    mining_elapsed_.restart();

    mining_state_->setText("Mining");
    mining_button_->setText("Stop mining");
    mining_timer_->start();
    mine_once();
}

void MainWindow::mine_once()
{
    constexpr std::uint64_t kBatchAttempts{
        4'096U
    };

    const auto result =
        runtime_.mine_wallet_block(
            kBatchAttempts
        );

    mining_total_attempts_ +=
        result.mining.attempts;

    if (result.ok()) {
        ++mining_blocks_found_;
    } else if (result.error !=
               NodeMineError::
                   proof_of_work_exhausted) {
        mining_timer_->stop();
        mining_state_->setText("Error");
        mining_button_->setText(
            "Start mining"
        );

        QMessageBox::warning(
            this,
            "Mining stopped",
            mining_error_text(
                result.error
            )
        );
    }

    const qint64 elapsed_ms =
        std::max<qint64>(
            mining_elapsed_.elapsed(),
            1
        );

    const double hashes_per_second =
        static_cast<double>(
            mining_total_attempts_
        ) *
        1000.0 /
        static_cast<double>(
            elapsed_ms
        );

    mining_hashrate_->setText(
        QString("%1 H/s")
            .arg(
                hashes_per_second,
                0,
                'f',
                hashes_per_second < 100.0
                    ? 1
                    : 0
            )
    );
    mining_attempts_->setText(
        QString::number(
            static_cast<qulonglong>(
                mining_total_attempts_
            )
        )
    );
    mining_blocks_->setText(
        QString::number(
            static_cast<qulonglong>(
                mining_blocks_found_
            )
        )
    );

    if (mining_timer_->isActive()) {
        mining_state_->setText("Mining");
    }
}

void MainWindow::show_recovery_phrase()
{
    bool accepted{false};

    QString password =
        QInputDialog::getText(
            this,
            "Verify wallet password",
            "Enter the wallet password before revealing the 24 recovery words:",
            QLineEdit::Password,
            {},
            &accepted
        );

    if (!accepted) {
        return;
    }

    QByteArray password_utf8 =
        password.toUtf8();

    const bool verified =
        runtime_.verify_wallet_passphrase(
            std::string_view{
                password_utf8.constData(),
                static_cast<std::size_t>(
                    password_utf8.size()
                )
            }
        );

    password.fill(QChar{0});
    password.clear();

    if (!password_utf8.isEmpty()) {
        crypto::secure_erase(
            std::span<Byte>{
                reinterpret_cast<Byte*>(
                    password_utf8.data()),
                static_cast<std::size_t>(
                    password_utf8.size()
                )
            }
        );
        password_utf8.clear();
    }

    if (!verified) {
        QMessageBox::warning(
            this,
            "Password incorrect",
            "The recovery words were not revealed."
        );
        return;
    }

    auto mnemonic =
        runtime_.wallet_recovery_mnemonic();

    if (!mnemonic) {
        QMessageBox::warning(
            this,
            "Recovery words unavailable",
            "This wallet cannot be recovered completely from a seed phrase alone. Keep a secure wallet.dat backup."
        );
        return;
    }

    const auto warning =
        QMessageBox::warning(
            this,
            "Reveal recovery words",
            "Anyone who gets these 24 words can control the recoverable coins in this wallet. "
            "Do not photograph them, send them in chat, or store them in cloud notes.\n\nReveal now?",
            QMessageBox::Yes |
                QMessageBox::Cancel,
            QMessageBox::Cancel
        );

    if (warning != QMessageBox::Yes) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(
        "QUINTUM 24 recovery words"
    );
    dialog.setMinimumWidth(620);

    auto* layout = new QVBoxLayout(&dialog);
    auto* text = new QTextEdit;
    text->setReadOnly(true);
    text->setPlainText(
        QString::fromStdString(
            *mnemonic
        )
    );

    auto* note = new QLabel(
        "Write these words down offline, in order. QUINTUM will never ask you to send them to anyone."
    );
    note->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Close
    );

    connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
    );

    layout->addWidget(note);
    layout->addWidget(text);
    layout->addWidget(buttons);

    dialog.exec();

    text->clear();

    auto& words = *mnemonic;

    crypto::secure_erase(
        std::span<Byte>{
            reinterpret_cast<Byte*>(
                words.data()),
            words.size()
        }
    );
    words.clear();
}

void MainWindow::backup_wallet_bundle()
{
    const QString destination =
        QFileDialog::getSaveFileName(
            this,
            "Backup complete QUINTUM wallet",
            "quintum-wallet-backup.qtmbackup",
            "QUINTUM backup (*.qtmbackup);;All files (*)"
        );

    if (destination.isEmpty()) {
        return;
    }

    const auto path =
        filesystem_path(destination);

    auto result =
        runtime_.backup_wallet_bundle(
            path,
            false
        );

    if (result ==
        wallet::WalletStoreError::
            target_exists) {
        const auto replace =
            QMessageBox::question(
                this,
                "Replace existing backup",
                "That backup file already exists. Replace it with a new complete backup?",
                QMessageBox::Yes |
                    QMessageBox::Cancel,
                QMessageBox::Cancel
            );

        if (replace != QMessageBox::Yes) {
            return;
        }

        result =
            runtime_.backup_wallet_bundle(
                path,
                true
            );
    }

    using wallet::WalletStoreError;

    if (result == WalletStoreError::none) {
        QMessageBox::information(
            this,
            "Wallet backed up",
            "Complete QUINTUM backup created successfully. Keep the .qtmbackup file offline and protected."
        );
        return;
    }

    QString error =
        "The complete wallet backup could not be written safely.";

    if (result ==
        WalletStoreError::wrong_network) {
        error =
            "The backup network does not match this wallet.";
    } else if (result ==
               WalletStoreError::unsafe_destination) {
        error =
            "Choose a backup file outside the live wallet.dat, wallet_state.dat and wallet_meta.dat files. QUINTUM will never overwrite active wallet data with a backup bundle.";
    }

    QMessageBox::warning(
        this,
        "Backup failed",
        error
    );
}

QString MainWindow::fee_quote_error_text(
    const wallet::WalletFeeQuote& quote)
{
    using wallet::WalletCreateError;

    switch (quote.error) {
    case WalletCreateError::none:
        return {};
    case WalletCreateError::not_started:
        return "The wallet is not running.";
    case WalletCreateError::sync_failed:
        return "The wallet could not synchronize before creating the payment.";
    case WalletCreateError::invalid_address:
        return "The destination is not a valid QUINTUM address.";
    case WalletCreateError::wrong_network_address:
        return "The destination belongs to another QUINTUM network.";
    case WalletCreateError::zero_amount:
        return "The amount must be greater than zero.";
    case WalletCreateError::amount_out_of_range:
        return "The amount is outside the valid QUINTUM money range.";
    case WalletCreateError::fee_out_of_range:
        return "The calculated fee is outside the valid range.";
    case WalletCreateError::value_overflow:
        return "The amount plus fee is too large.";
    case WalletCreateError::insufficient_funds:
        return "The wallet does not have enough mature available funds for this payment and fee.";
    case WalletCreateError::key_generation_failed:
        return "The wallet could not reserve a change key.";
    case WalletCreateError::store_failed:
        return "The wallet could not safely persist its key state.";
    case WalletCreateError::missing_private_key:
        return "A required private key is missing from this wallet.";
    case WalletCreateError::signing_failed:
        return "The wallet could not sign the transaction.";
    case WalletCreateError::validation_failed:
        return "The completed transaction failed local validation.";
    }

    return "The payment cannot be created.";
}

QString MainWindow::mining_error_text(
    NodeMineError error)
{
    switch (error) {
    case NodeMineError::none:
        return {};
    case NodeMineError::not_started:
        return "The node is not running.";
    case NodeMineError::template_failed:
        return "A valid block template could not be created for this wallet.";
    case NodeMineError::proof_of_work_exhausted:
        return "No block was found in this mining batch.";
    case NodeMineError::proof_of_work_invalid:
        return "The generated proof of work failed validation.";
    case NodeMineError::chain_rejected:
        return "The mined block was rejected by local chain validation.";
    case NodeMineError::storage_failed:
        return "The mined block could not be saved safely to disk.";
    }

    return "Mining failed.";
}

QString MainWindow::format_amount(
    Amount value)
{
    const Amount whole =
        value /
        consensus::kAtomicUnitsPerCoin;
    const Amount fraction =
        value %
        consensus::kAtomicUnitsPerCoin;

    return QString("%1.%2")
        .arg(
            static_cast<qulonglong>(
                whole
            )
        )
        .arg(
            static_cast<qulonglong>(
                fraction
            ),
            8,
            10,
            QChar('0')
        );
}

std::optional<Amount>
MainWindow::parse_amount(
    const QString& value)
{
    const QString text =
        value.trimmed();

    if (text.isEmpty() ||
        text.startsWith('-') ||
        text.startsWith('+')) {
        return std::nullopt;
    }

    const QStringList parts =
        text.split('.');

    if (parts.size() > 2 ||
        parts.front().isEmpty()) {
        return std::nullopt;
    }

    bool whole_ok{false};
    const qulonglong whole =
        parts.front().toULongLong(
            &whole_ok,
            10
        );

    if (!whole_ok) {
        return std::nullopt;
    }

    QString fraction_text =
        parts.size() == 2
            ? parts.back()
            : QString{};

    if (fraction_text.size() > 8) {
        return std::nullopt;
    }

    for (const QChar ch : fraction_text) {
        if (!ch.isDigit()) {
            return std::nullopt;
        }
    }

    while (fraction_text.size() < 8) {
        fraction_text.append('0');
    }

    bool fraction_ok{true};
    qulonglong fraction{0U};

    if (!fraction_text.isEmpty()) {
        fraction =
            fraction_text.toULongLong(
                &fraction_ok,
                10
            );
    }

    if (!fraction_ok) {
        return std::nullopt;
    }

    const Amount unit =
        consensus::kAtomicUnitsPerCoin;

    if (whole >
        std::numeric_limits<Amount>::max() /
            unit) {
        return std::nullopt;
    }

    const Amount base =
        static_cast<Amount>(whole) *
        unit;

    if (fraction >
        std::numeric_limits<Amount>::max() -
            base) {
        return std::nullopt;
    }

    const Amount total =
        base +
        static_cast<Amount>(fraction);

    if (!consensus::money_range(total)) {
        return std::nullopt;
    }

    return total;
}

QString MainWindow::hash_hex(
    const Hash256& hash)
{
    static constexpr std::array<char, 16>
        digits{
            '0', '1', '2', '3',
            '4', '5', '6', '7',
            '8', '9', 'a', 'b',
            'c', 'd', 'e', 'f',
        };

    QString out;
    out.reserve(
        static_cast<qsizetype>(
            hash.size() * 2U
        )
    );

    for (const Byte byte : hash) {
        out.append(
            QChar(
                digits[
                    static_cast<std::size_t>(
                        byte >> 4U
                    )
                ]
            )
        );
        out.append(
            QChar(
                digits[
                    static_cast<std::size_t>(
                        byte & 0x0fU
                    )
                ]
            )
        );
    }

    return out;
}

} // namespace quintum::qtui
