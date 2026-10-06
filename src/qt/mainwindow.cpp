#include "qt/mainwindow.hpp"

#include "consensus/monetary.hpp"
#include "crypto/random.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QStackedWidget>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>

namespace quintum::qtui {
namespace {

QString app_stylesheet()
{
    return QString::fromUtf8(R"QSS(
QMainWindow {
    background: #f8fbff;
}
QWidget {
    color: #17243d;
    font-family: "Segoe UI", "Inter", sans-serif;
    font-size: 14px;
}
QWidget#appRoot {
    background: #f8fbff;
}
QListWidget#navigation {
    background: #f3f8fe;
    border: none;
    border-right: 1px solid #dbe5f1;
    padding: 18px 7px;
    outline: none;
}
QListWidget#navigation::item {
    color: #273956;
    height: 54px;
    margin: 3px 5px;
    padding-left: 13px;
    border-radius: 10px;
}
QListWidget#navigation::item:hover {
    background: #edf5ff;
}
QListWidget#navigation::item:selected {
    color: #0869e8;
    background: #e2f0ff;
    font-weight: 600;
}
QLabel#pageTitle {
    color: #080f24;
    font-size: 34px;
    font-weight: 700;
}
QLabel#pageSubtitle {
    color: #526785;
    font-size: 16px;
}
QLabel#sectionTitle {
    color: #101b31;
    font-size: 18px;
    font-weight: 700;
}
QLabel#sectionSubtitle {
    color: #617493;
    font-size: 13px;
}
QFrame#card, QFrame#sectionCard, QFrame#metricCard {
    background: #ffffff;
    border: 1px solid #d8e4f0;
    border-radius: 13px;
}
QFrame#metricCard[tone="blue"] {
    background: #eef7ff;
    border-color: #cfe4fa;
}
QFrame#metricCard[tone="green"] {
    background: #effcf4;
    border-color: #d2eedc;
}
QFrame#metricCard[tone="amber"] {
    background: #fff8e9;
    border-color: #f4e1b2;
}
QFrame#metricCard[tone="purple"] {
    background: #f6f1ff;
    border-color: #e2d7fb;
}
QLabel#metricIcon {
    min-width: 46px;
    max-width: 46px;
    min-height: 46px;
    max-height: 46px;
    border-radius: 23px;
    font-size: 22px;
    font-weight: 700;
    qproperty-alignment: AlignCenter;
}
QLabel#metricIcon[tone="blue"] {
    background: #d9edff;
    color: #056ce8;
}
QLabel#metricIcon[tone="green"] {
    background: #d9f7e4;
    color: #0a9e4a;
}
QLabel#metricIcon[tone="amber"] {
    background: #ffebba;
    color: #df8a00;
}
QLabel#metricIcon[tone="purple"] {
    background: #e9defd;
    color: #6631c8;
}
QLabel#metricCaption {
    color: #263a59;
    font-size: 14px;
    font-weight: 600;
}
QLabel#metricValue {
    color: #091226;
    font-size: 24px;
    font-weight: 700;
}
QLabel#metricUnit, QLabel#muted {
    color: #60738f;
    font-size: 13px;
}
QLineEdit, QComboBox {
    background: #ffffff;
    border: 1px solid #cad9e9;
    border-radius: 8px;
    min-height: 38px;
    padding: 0 11px;
    selection-background-color: #0c72ee;
}
QLineEdit:focus, QComboBox:focus {
    border: 1px solid #0c72ee;
}
QPushButton {
    background: #f6f9fd;
    color: #1e4e87;
    border: 1px solid #cbdced;
    border-radius: 8px;
    min-height: 40px;
    padding: 0 17px;
    font-weight: 600;
}
QPushButton:hover {
    background: #edf5ff;
    border-color: #9dc3eb;
}
QPushButton:disabled {
    color: #9aa9bb;
    background: #f6f8fa;
    border-color: #e1e7ee;
}
QPushButton#primaryButton {
    color: white;
    background: #0b70ee;
    border: 1px solid #0b70ee;
    min-height: 44px;
    font-size: 15px;
}
QPushButton#primaryButton:hover {
    background: #0565dc;
}
QProgressBar {
    background: #e7edf5;
    border: 1px solid #cfdae8;
    border-radius: 7px;
    min-height: 14px;
    max-height: 14px;
    text-align: center;
}
QProgressBar::chunk {
    border-radius: 6px;
    background: #0caf58;
}
QTableWidget {
    background: #ffffff;
    alternate-background-color: #f9fbfe;
    border: 1px solid #d7e3ef;
    border-radius: 10px;
    gridline-color: #e4ebf3;
    selection-background-color: #e7f2ff;
    selection-color: #15243d;
}
QTableWidget::item {
    padding: 6px;
}
QHeaderView::section {
    color: #223754;
    background: #f1f7fd;
    border: none;
    border-right: 1px solid #dce7f2;
    border-bottom: 1px solid #dce7f2;
    padding: 9px 7px;
    font-weight: 700;
}
QScrollArea {
    border: none;
    background: transparent;
}
QStatusBar {
    background: #f8fbff;
    border-top: 1px solid #dbe5f1;
    min-height: 42px;
}
QStatusBar QLabel {
    color: #263a59;
    padding: 0 12px;
}
QLabel#statusRunning {
    color: #079b43;
    font-weight: 700;
}
QLabel#statusStopped {
    color: #c2413b;
    font-weight: 700;
}
QCheckBox {
    spacing: 8px;
}
)QSS");
}

QWidget* make_page_header(
    const QString& title_text,
    const QString& subtitle_text)
{
    auto* header = new QWidget;
    auto* layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 2);
    layout->setSpacing(2);

    auto* title = new QLabel(title_text);
    title->setObjectName("pageTitle");

    auto* subtitle = new QLabel(subtitle_text);
    subtitle->setObjectName("pageSubtitle");
    subtitle->setWordWrap(true);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    return header;
}

QFrame* make_card()
{
    auto* frame = new QFrame;
    frame->setObjectName("card");
    return frame;
}

QLabel* make_metric_icon(
    const QString& glyph,
    const QString& tone)
{
    auto* label = new QLabel(glyph);
    label->setObjectName("metricIcon");
    label->setProperty(
        "tone",
        tone
    );
    return label;
}

QFrame* make_balance_card(
    const QString& title,
    const QString& glyph,
    const QString& tone,
    QLabel*& value)
{
    auto* frame = new QFrame;
    frame->setObjectName("metricCard");
    frame->setProperty("tone", tone);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 17, 18, 17);
    layout->setSpacing(7);

    layout->addWidget(
        make_metric_icon(
            glyph,
            tone
        ),
        0,
        Qt::AlignLeft
    );

    auto* caption = new QLabel(title);
    caption->setObjectName("metricCaption");

    value = new QLabel("0.00000000");
    value->setObjectName("metricValue");

    auto* unit = new QLabel("QMU");
    unit->setObjectName("metricUnit");

    layout->addWidget(caption);
    layout->addWidget(value);
    layout->addWidget(unit);
    layout->addStretch();

    return frame;
}

QFrame* make_mining_metric(
    const QString& title,
    const QString& glyph,
    const QString& tone,
    const QString& subtext,
    QLabel*& value)
{
    auto* frame = new QFrame;
    frame->setObjectName("metricCard");
    frame->setProperty("tone", tone);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(6);

    auto* top = new QHBoxLayout;
    top->setSpacing(10);
    top->addWidget(
        make_metric_icon(glyph, tone),
        0
    );

    auto* caption = new QLabel(title);
    caption->setObjectName("metricCaption");
    top->addWidget(caption, 1);
    layout->addLayout(top);

    value = new QLabel("-");
    value->setObjectName("metricValue");
    layout->addWidget(value);

    auto* detail = new QLabel(subtext);
    detail->setObjectName("muted");
    detail->setWordWrap(true);
    layout->addWidget(detail);

    return frame;
}

QWidget* make_section_heading(
    const QString& title_text,
    const QString& subtitle_text)
{
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* title = new QLabel(title_text);
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    if (!subtitle_text.isEmpty()) {
        auto* subtitle = new QLabel(subtitle_text);
        subtitle->setObjectName("sectionSubtitle");
        subtitle->setWordWrap(true);
        layout->addWidget(subtitle);
    }

    return widget;
}

QFrame* make_separator()
{
    auto* separator = new QFrame;
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setStyleSheet(
        "color:#d7e2ed;"
    );
    return separator;
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
    setWindowTitle(
        QString("QUINTUM Core — %1")
            .arg(QString::fromUtf8(params_.name))
    );
    setWindowIcon(
        QIcon(":/branding/quintum_icon.png")
    );
    QApplication::setWindowIcon(
        QIcon(":/branding/quintum_icon.png")
    );

    resize(1280, 820);
    setMinimumSize(1040, 680);
    setStyleSheet(app_stylesheet());

    auto* root = new QWidget;
    root->setObjectName("appRoot");

    auto* layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* navigation = new QListWidget;
    navigation->setObjectName("navigation");
    navigation->setFixedWidth(220);
    navigation->setIconSize(QSize(28, 28));
    navigation->setSpacing(3);

    const std::array<std::pair<QString, QString>, 7>
        nav_items{{
            {":/icons/home.svg", "Overview"},
            {":/icons/send.svg", "Send"},
            {":/icons/receive.svg", "Receive"},
            {":/icons/transactions.svg", "Transactions"},
            {":/icons/addressbook.svg", "Address Book"},
            {":/icons/mining.svg", "Mining"},
            {":/icons/settings.svg", "Settings"},
        }};

    for (const auto& [icon_path, title] :
         nav_items) {
        auto* item =
            new QListWidgetItem(
                QIcon(icon_path),
                title
            );
        item->setSizeHint(QSize(200, 56));
        navigation->addItem(item);
    }

    navigation->setCurrentRow(0);

    pages_ = new QStackedWidget;
    pages_->addWidget(build_overview_page());
    pages_->addWidget(build_send_page());
    pages_->addWidget(build_receive_page());
    pages_->addWidget(build_transactions_page());
    pages_->addWidget(build_address_book_page());
    pages_->addWidget(build_mining_page());
    pages_->addWidget(build_settings_page());

    connect(
        navigation,
        &QListWidget::currentRowChanged,
        pages_,
        &QStackedWidget::setCurrentIndex
    );

    layout->addWidget(navigation);
    layout->addWidget(pages_, 1);
    setCentralWidget(root);

    status_network_ = new QLabel;
    status_blocks_ = new QLabel;
    status_peers_ = new QLabel;
    status_connection_ = new QLabel;

    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(
        status_network_,
        1
    );
    statusBar()->addPermanentWidget(
        make_separator()
    );
    statusBar()->addPermanentWidget(
        status_blocks_
    );
    statusBar()->addPermanentWidget(
        make_separator()
    );
    statusBar()->addPermanentWidget(
        status_peers_
    );
    statusBar()->addPermanentWidget(
        make_separator()
    );
    statusBar()->addPermanentWidget(
        status_connection_
    );

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
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(18);

    layout->addWidget(
        make_page_header(
            "Overview",
            "Your QUINTUM wallet and network status at a glance."
        )
    );

    auto* balances = new QHBoxLayout;
    balances->setSpacing(14);
    balances->addWidget(
        make_balance_card(
            "Available",
            "▣",
            "blue",
            available_value_
        ),
        1
    );
    balances->addWidget(
        make_balance_card(
            "Confirmed",
            "≡",
            "green",
            confirmed_value_
        ),
        1
    );
    balances->addWidget(
        make_balance_card(
            "Pending",
            "◷",
            "amber",
            pending_value_
        ),
        1
    );
    balances->addWidget(
        make_balance_card(
            "Immature",
            "□",
            "purple",
            immature_value_
        ),
        1
    );
    layout->addLayout(balances);

    auto* network_frame = make_card();
    auto* network_layout =
        new QVBoxLayout(network_frame);
    network_layout->setContentsMargins(
        22,
        19,
        22,
        20
    );
    network_layout->setSpacing(16);

    network_layout->addWidget(
        make_section_heading(
            "Synchronization",
            "Network and blockchain status"
        )
    );

    auto* detail_grid = new QGridLayout;
    detail_grid->setHorizontalSpacing(22);
    detail_grid->setVerticalSpacing(12);
    detail_grid->setColumnStretch(1, 1);
    detail_grid->setColumnStretch(3, 1);

    overview_height_ = new QLabel("-");
    overview_peer_height_ = new QLabel("-");
    overview_peers_ = new QLabel("0");
    overview_mempool_ = new QLabel("0");
    overview_sync_state_ = new QLabel("Starting");

    auto add_detail =
        [detail_grid](
            int row,
            int column,
            const QString& label_text,
            QLabel* value) {
            auto* label =
                new QLabel(label_text);
            label->setObjectName("muted");

            QFont font = value->font();
            font.setBold(true);
            value->setFont(font);

            detail_grid->addWidget(
                label,
                row,
                column
            );
            detail_grid->addWidget(
                value,
                row,
                column + 1
            );
        };

    add_detail(
        0,
        0,
        "Local block height:",
        overview_height_
    );
    add_detail(
        1,
        0,
        "Peer best height:",
        overview_peer_height_
    );
    add_detail(
        2,
        0,
        "Peers:",
        overview_peers_
    );
    add_detail(
        0,
        2,
        "Mempool:",
        overview_mempool_
    );
    add_detail(
        1,
        2,
        "Synchronization:",
        overview_sync_state_
    );

    network_layout->addLayout(detail_grid);

    auto* progress_label =
        new QLabel("Blockchain progress:");
    progress_label->setObjectName(
        "metricCaption"
    );
    network_layout->addWidget(progress_label);

    auto* progress_row = new QHBoxLayout;
    overview_sync_progress_ =
        new QProgressBar;
    overview_sync_progress_->setRange(
        0,
        100
    );
    overview_sync_progress_->setValue(0);
    overview_sync_progress_->setTextVisible(
        false
    );

    auto* progress_percent =
        new QLabel("0%");
    progress_percent->setObjectName(
        "muted"
    );

    connect(
        overview_sync_progress_,
        &QProgressBar::valueChanged,
        progress_percent,
        [progress_percent](int value) {
            progress_percent->setText(
                QString::number(value) + "%"
            );
        }
    );

    progress_row->addWidget(
        overview_sync_progress_,
        1
    );
    progress_row->addWidget(
        progress_percent
    );
    network_layout->addLayout(progress_row);

    layout->addWidget(network_frame);
    layout->addStretch();

    return page;
}

QWidget* MainWindow::build_send_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(18);

    layout->addWidget(
        make_page_header(
            "Send",
            "Send QMU to another wallet address."
        )
    );

    auto* recipient = make_card();
    auto* recipient_layout =
        new QVBoxLayout(recipient);
    recipient_layout->setContentsMargins(
        22,
        18,
        22,
        20
    );
    recipient_layout->setSpacing(13);

    auto* recipient_header = new QHBoxLayout;
    recipient_header->addWidget(
        make_section_heading(
            "Recipient",
            {}
        )
    );
    recipient_header->addStretch();

    auto* add_recipient =
        new QPushButton("+  Add recipient");
    add_recipient->setEnabled(false);
    add_recipient->setToolTip(
        "Multi-recipient transactions are intentionally disabled during Testnet validation."
    );
    recipient_header->addWidget(
        add_recipient
    );
    recipient_layout->addLayout(
        recipient_header
    );

    auto* form = new QGridLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(12);
    form->setColumnStretch(1, 1);

    send_address_ = new QLineEdit;
    send_address_->setPlaceholderText(
        "Enter a QMU address"
    );

    send_label_ = new QLineEdit;
    send_label_->setPlaceholderText(
        "e.g. Friend, Savings, Exchange..."
    );

    send_amount_ = new QLineEdit;
    send_amount_->setPlaceholderText(
        "0.00000000"
    );

    form->addWidget(
        new QLabel("Pay to"),
        0,
        0
    );
    form->addWidget(
        send_address_,
        0,
        1,
        1,
        2
    );
    form->addWidget(
        new QLabel("Label (optional)"),
        1,
        0
    );
    form->addWidget(
        send_label_,
        1,
        1,
        1,
        2
    );
    form->addWidget(
        new QLabel("Amount (QMU)"),
        2,
        0
    );
    form->addWidget(
        send_amount_,
        2,
        1
    );

    auto* unit = new QLabel("QMU");
    unit->setObjectName("metricCaption");
    form->addWidget(
        unit,
        2,
        2
    );

    recipient_layout->addLayout(form);
    layout->addWidget(recipient);

    auto* fee_card = make_card();
    auto* fee_layout =
        new QVBoxLayout(fee_card);
    fee_layout->setContentsMargins(
        22,
        18,
        22,
        20
    );
    fee_layout->setSpacing(14);

    fee_layout->addWidget(
        make_section_heading(
            "Transaction Fee",
            "QUINTUM calculates the safe relay fee automatically."
        )
    );

    auto* fee_options = new QHBoxLayout;
    fee_options->setSpacing(14);

    auto* recommended = new QFrame;
    recommended->setObjectName(
        "metricCard"
    );
    recommended->setProperty(
        "tone",
        "blue"
    );

    auto* recommended_layout =
        new QVBoxLayout(recommended);
    recommended_layout->setContentsMargins(
        18,
        15,
        18,
        15
    );
    recommended_layout->addWidget(
        new QLabel("●  Recommended")
    );

    send_fee_rate_ = new QLabel("-");
    send_fee_rate_->setObjectName(
        "metricValue"
    );
    recommended_layout->addWidget(
        send_fee_rate_
    );

    auto* estimate =
        new QLabel(
            "Target block interval: ~10 minutes"
        );
    estimate->setObjectName("muted");
    recommended_layout->addWidget(
        estimate
    );

    auto* custom = new QFrame;
    custom->setObjectName("card");
    auto* custom_layout =
        new QVBoxLayout(custom);
    custom_layout->setContentsMargins(
        18,
        15,
        18,
        15
    );
    custom_layout->addWidget(
        new QLabel("○  Custom")
    );
    auto* custom_note =
        new QLabel(
            "Disabled while Testnet fee policy is being validated."
        );
    custom_note->setObjectName("muted");
    custom_note->setWordWrap(true);
    custom_layout->addWidget(
        custom_note
    );

    fee_options->addWidget(
        recommended,
        1
    );
    fee_options->addWidget(
        custom,
        1
    );
    fee_layout->addLayout(fee_options);

    auto* policy =
        new QLabel(
            "Fee is added to the entered amount. The final amount, fee and total are shown before signing."
        );
    policy->setObjectName("muted");
    policy->setWordWrap(true);
    fee_layout->addWidget(policy);

    auto* action_row = new QHBoxLayout;
    action_row->addStretch();

    auto* clear = new QPushButton("Clear");
    connect(
        clear,
        &QPushButton::clicked,
        this,
        [this] {
            send_address_->clear();
            send_amount_->clear();
            send_label_->clear();
        }
    );

    send_button_ = new QPushButton(
        "Send"
    );
    send_button_->setObjectName(
        "primaryButton"
    );
    send_button_->setMinimumWidth(190);

    connect(
        send_button_,
        &QPushButton::clicked,
        this,
        [this] {
            preview_and_send();
        }
    );

    action_row->addWidget(clear);
    action_row->addWidget(send_button_);
    fee_layout->addLayout(action_row);

    layout->addWidget(fee_card);
    layout->addStretch();

    return page;
}

QWidget* MainWindow::build_receive_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(18);

    layout->addWidget(
        make_page_header(
            "Receive",
            "Receive QMU to your wallet using the address below."
        )
    );

    auto* main_row = new QHBoxLayout;
    main_row->setSpacing(14);

    auto* address_card = make_card();
    auto* address_layout =
        new QVBoxLayout(address_card);
    address_layout->setContentsMargins(
        22,
        18,
        22,
        20
    );
    address_layout->setSpacing(13);

    address_layout->addWidget(
        make_section_heading(
            "Your receive address",
            "Share this address to receive QMU payments."
        )
    );

    auto* address_row = new QHBoxLayout;

    receive_address_ = new QLineEdit;
    receive_address_->setReadOnly(true);

    auto* copy = new QPushButton(
        "Copy"
    );
    copy->setToolTip(
        "Copy receive address"
    );

    connect(
        copy,
        &QPushButton::clicked,
        this,
        [this] {
            copy_receive_address();
        }
    );

    address_row->addWidget(
        receive_address_,
        1
    );
    address_row->addWidget(copy);
    address_layout->addLayout(
        address_row
    );

    auto* lower_row = new QHBoxLayout;
    lower_row->setSpacing(18);

    auto* brand_panel = new QFrame;
    brand_panel->setObjectName("metricCard");
    brand_panel->setProperty(
        "tone",
        "blue"
    );
    brand_panel->setFixedSize(
        205,
        205
    );

    auto* brand_layout =
        new QVBoxLayout(brand_panel);
    auto* brand =
        new QLabel;
    brand->setPixmap(
        QIcon(":/branding/quintum_icon.png")
            .pixmap(112, 112)
    );
    brand->setAlignment(
        Qt::AlignCenter
    );

    auto* qr_note =
        new QLabel(
            "QMU address\nQR after payment-URI freeze"
        );
    qr_note->setAlignment(
        Qt::AlignCenter
    );
    qr_note->setObjectName(
        "muted"
    );

    brand_layout->addStretch();
    brand_layout->addWidget(brand);
    brand_layout->addWidget(qr_note);
    brand_layout->addStretch();

    auto* receive_actions =
        new QVBoxLayout;

    auto* copy_large =
        new QPushButton(
            "Copy address"
        );
    copy_large->setObjectName(
        "primaryButton"
    );

    connect(
        copy_large,
        &QPushButton::clicked,
        this,
        [this] {
            copy_receive_address();
        }
    );

    auto* fresh =
        new QPushButton(
            "Generate new address"
        );

    connect(
        fresh,
        &QPushButton::clicked,
        this,
        [this] {
            new_receive_address();
        }
    );

    auto* helper =
        new QLabel(
            "Share this address with the sender. Payments appear in the wallet after they are relayed and confirmed by the network."
        );
    helper->setObjectName("muted");
    helper->setWordWrap(true);

    receive_actions->addWidget(
        copy_large
    );
    receive_actions->addWidget(fresh);
    receive_actions->addWidget(helper);
    receive_actions->addStretch();

    lower_row->addWidget(brand_panel);
    lower_row->addLayout(
        receive_actions,
        1
    );
    address_layout->addLayout(
        lower_row
    );

    main_row->addWidget(
        address_card,
        2
    );

    auto* status_card = make_card();
    auto* status_layout =
        new QVBoxLayout(status_card);
    status_layout->setContentsMargins(
        18,
        18,
        18,
        18
    );
    status_layout->setSpacing(10);
    status_layout->addWidget(
        make_section_heading(
            "Receiving status",
            "Incoming payments to this wallet."
        )
    );

    auto add_status =
        [status_layout](
            const QString& title,
            const QString& tone,
            QLabel*& value) {
            auto* row = new QFrame;
            row->setObjectName(
                "metricCard"
            );
            row->setProperty(
                "tone",
                tone
            );

            auto* l =
                new QVBoxLayout(row);
            l->setContentsMargins(
                14,
                11,
                14,
                11
            );

            auto* caption =
                new QLabel(title);
            caption->setObjectName(
                "metricCaption"
            );
            value =
                new QLabel(
                    "0.00000000 QMU"
                );
            value->setObjectName(
                "metricValue"
            );

            l->addWidget(caption);
            l->addWidget(value);
            status_layout->addWidget(row);
        };

    add_status(
        "Confirmed",
        "green",
        receive_confirmed_
    );
    add_status(
        "Pending",
        "amber",
        receive_pending_
    );
    add_status(
        "Total received",
        "blue",
        receive_total_
    );
    status_layout->addStretch();

    main_row->addWidget(
        status_card,
        1
    );

    layout->addLayout(main_row);

    auto* label_card = make_card();
    auto* label_layout =
        new QVBoxLayout(label_card);
    label_layout->setContentsMargins(
        22,
        16,
        22,
        18
    );
    label_layout->setSpacing(9);

    label_layout->addWidget(
        make_section_heading(
            "Address label (optional)",
            "Save a local label in your address book for this receive address."
        )
    );

    auto* label_row = new QHBoxLayout;
    receive_label_ = new QLineEdit;
    receive_label_->setPlaceholderText(
        "My main receiving address"
    );

    auto* save_label =
        new QPushButton("Save label");

    connect(
        save_label,
        &QPushButton::clicked,
        this,
        [this] {
            save_receive_label();
        }
    );

    label_row->addWidget(
        receive_label_,
        1
    );
    label_row->addWidget(
        save_label
    );
    label_layout->addLayout(label_row);

    layout->addWidget(label_card);
    layout->addStretch();

    return page;
}

QWidget* MainWindow::build_transactions_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(16);

    layout->addWidget(
        make_page_header(
            "Transactions",
            "View and manage your QUINTUM transaction history."
        )
    );

    auto* filters = new QHBoxLayout;
    filters->setSpacing(10);

    transaction_search_ = new QLineEdit;
    transaction_search_->setPlaceholderText(
        "Search transactions (TxID, label, status)..."
    );

    auto* type_filter = new QComboBox;
    type_filter->addItems({
        "All types",
        "Received",
        "Spent",
        "Mining rewards",
    });
    type_filter->setEnabled(false);
    type_filter->setToolTip(
        "Type filtering will be enabled after the transaction detail model is finalized."
    );

    transaction_status_filter_ =
        new QComboBox;
    transaction_status_filter_->addItems({
        "All statuses",
        "Confirmed",
        "Pending",
        "Inactive",
    });

    auto* time_filter = new QComboBox;
    time_filter->addItem("All time");
    time_filter->setEnabled(false);

    filters->addWidget(
        transaction_search_,
        1
    );
    filters->addWidget(type_filter);
    filters->addWidget(
        transaction_status_filter_
    );
    filters->addWidget(time_filter);
    layout->addLayout(filters);

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
    transactions_->setAlternatingRowColors(
        true
    );
    transactions_->setShowGrid(true);
    transactions_->verticalHeader()->setVisible(
        false
    );
    transactions_->horizontalHeader()
        ->setStretchLastSection(true);
    transactions_->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::ResizeToContents
        );
    transactions_->horizontalHeader()
        ->setSectionResizeMode(
            1,
            QHeaderView::Stretch
        );

    connect(
        transaction_search_,
        &QLineEdit::textChanged,
        this,
        [this] {
            filter_transactions();
        }
    );

    connect(
        transaction_status_filter_,
        &QComboBox::currentTextChanged,
        this,
        [this] {
            filter_transactions();
        }
    );

    layout->addWidget(
        transactions_,
        1
    );

    return page;
}

QWidget* MainWindow::build_address_book_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(16);

    layout->addWidget(
        make_page_header(
            "Address Book",
            "Keep local labels for people, services and your own QUINTUM addresses."
        )
    );

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
    address_book_table_->setAlternatingRowColors(
        true
    );
    address_book_table_->verticalHeader()
        ->setVisible(false);
    address_book_table_->horizontalHeader()
        ->setSectionResizeMode(
            0,
            QHeaderView::Stretch
        );
    address_book_table_->horizontalHeader()
        ->setSectionResizeMode(
            1,
            QHeaderView::ResizeToContents
        );

    layout->addWidget(
        address_book_table_,
        1
    );

    auto* editor = make_card();
    auto* editor_layout =
        new QVBoxLayout(editor);
    editor_layout->setContentsMargins(
        20,
        16,
        20,
        18
    );
    editor_layout->setSpacing(10);

    editor_layout->addWidget(
        make_section_heading(
            "Address label",
            "Select a row to edit it, or enter a new address and label."
        )
    );

    auto* form = new QGridLayout;
    form->setColumnStretch(1, 1);
    address_book_address_ =
        new QLineEdit;
    address_book_label_ =
        new QLineEdit;

    form->addWidget(
        new QLabel("Address"),
        0,
        0
    );
    form->addWidget(
        address_book_address_,
        0,
        1
    );
    form->addWidget(
        new QLabel("Label"),
        1,
        0
    );
    form->addWidget(
        address_book_label_,
        1,
        1
    );
    editor_layout->addLayout(form);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();

    auto* remove =
        new QPushButton("Delete");
    auto* save =
        new QPushButton("Save");
    save->setObjectName(
        "primaryButton"
    );

    buttons->addWidget(remove);
    buttons->addWidget(save);
    editor_layout->addLayout(buttons);

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

    layout->addWidget(editor);

    return page;
}

QWidget* MainWindow::build_mining_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);

    layout->addWidget(
        make_page_header(
            "Mining",
            "Help secure the QUINTUM network and earn valid Proof-of-Work block rewards."
        )
    );

    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(12);

    metrics->addWidget(
        make_mining_metric(
            "Status",
            "▷",
            "green",
            "Current miner state",
            mining_state_
        ),
        1
    );
    metrics->addWidget(
        make_mining_metric(
            "Hash rate",
            "⌁",
            "blue",
            "Current mining speed",
            mining_hashrate_
        ),
        1
    );
    metrics->addWidget(
        make_mining_metric(
            "Hashes attempted",
            "≋",
            "purple",
            "Total this session",
            mining_attempts_
        ),
        1
    );
    metrics->addWidget(
        make_mining_metric(
            "Blocks found",
            "◇",
            "green",
            "Total this session",
            mining_blocks_
        ),
        1
    );

    layout->addLayout(metrics);

    auto* secondary = new QHBoxLayout;
    secondary->setSpacing(12);

    secondary->addWidget(
        make_mining_metric(
            "Current difficulty",
            "▥",
            "amber",
            "Active-chain difficulty. 1.0 is the easiest target allowed by this network.",
            mining_difficulty_
        ),
        1
    );

    secondary->addWidget(
        make_mining_metric(
            "Estimated time per block",
            "◷",
            "blue",
            "Observed average for this mining session",
            mining_eta_
        ),
        1
    );

    layout->addLayout(secondary);

    auto* control = make_card();
    auto* control_layout =
        new QHBoxLayout(control);
    control_layout->setContentsMargins(
        20,
        17,
        20,
        17
    );
    control_layout->setSpacing(22);

    mining_button_ =
        new QPushButton("Start mining");
    mining_button_->setObjectName(
        "primaryButton"
    );
    mining_button_->setMinimumWidth(
        280
    );

    connect(
        mining_button_,
        &QPushButton::clicked,
        this,
        [this] {
            toggle_mining();
        }
    );

    control_layout->addWidget(
        mining_button_
    );
    control_layout->addWidget(
        make_separator()
    );

    auto* performance =
        make_section_heading(
            "RandomX performance",
            "Full-memory RandomX is preferred automatically; systems without enough RAM fall back safely to light mode."
        );
    control_layout->addWidget(
        performance,
        1
    );

    layout->addWidget(control);

    auto* activity_row = new QHBoxLayout;
    activity_row->setSpacing(12);

    auto* activity = make_card();
    auto* activity_layout =
        new QVBoxLayout(activity);
    activity_layout->setContentsMargins(
        18,
        15,
        18,
        16
    );

    activity_layout->addWidget(
        make_section_heading(
            "Recent activity",
            "Hash rate updates continuously while mining."
        )
    );

    auto* activity_line =
        new QFrame;
    activity_line->setFixedHeight(5);
    activity_line->setStyleSheet(
        "background:#0b70ee;border-radius:2px;"
    );
    activity_layout->addStretch();
    activity_layout->addWidget(
        activity_line
    );
    activity_layout->addStretch();

    activity_row->addWidget(
        activity,
        2
    );

    auto* blocks_card = make_card();
    auto* blocks_layout =
        new QVBoxLayout(blocks_card);
    blocks_layout->setContentsMargins(
        18,
        15,
        18,
        16
    );

    blocks_layout->addWidget(
        make_section_heading(
            "Recent blocks",
            "Blocks found by this mining session."
        )
    );

    mining_recent_blocks_ =
        new QTableWidget;
    mining_recent_blocks_->setColumnCount(
        3
    );
    mining_recent_blocks_
        ->setHorizontalHeaderLabels({
            "Height",
            "Reward",
            "Time",
        });
    mining_recent_blocks_
        ->verticalHeader()
        ->setVisible(false);
    mining_recent_blocks_->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );
    mining_recent_blocks_
        ->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Stretch
        );
    mining_recent_blocks_->setMaximumHeight(
        150
    );

    blocks_layout->addWidget(
        mining_recent_blocks_
    );

    activity_row->addWidget(
        blocks_card,
        1
    );

    layout->addLayout(
        activity_row,
        1
    );

    mining_state_->setText("Ready");
    mining_hashrate_->setText("0.00 H/s");
    mining_attempts_->setText("0");
    mining_blocks_->setText("0");
    mining_difficulty_->setText("—");
    mining_eta_->setText("—");

    return page;
}

QWidget* MainWindow::build_settings_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);

    layout->addWidget(
        make_page_header(
            "Settings",
            "Configure QUINTUM Core and manage your wallet."
        )
    );

    auto* network = make_card();
    auto* network_layout =
        new QVBoxLayout(network);
    network_layout->setContentsMargins(
        22,
        17,
        22,
        19
    );
    network_layout->setSpacing(12);
    network_layout->addWidget(
        make_section_heading(
            "Network",
            "Network connection and peer-to-peer settings"
        )
    );

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);

    settings_network_ = new QLabel;
    settings_p2p_port_ = new QLabel;
    settings_listen_port_ = new QLabel;
    settings_fee_rate_ = new QLabel;

    const std::array<
        std::pair<QString, QLabel*>,
        4> network_rows{{
            {"Network:", settings_network_},
            {"Default P2P port:", settings_p2p_port_},
            {"Listening port:", settings_listen_port_},
            {"Recommended fee rate:", settings_fee_rate_},
        }};

    for (std::size_t i = 0U;
         i < network_rows.size();
         ++i) {
        auto* label =
            new QLabel(
                network_rows[i].first
            );
        label->setObjectName("muted");

        auto* value =
            network_rows[i].second;
        QFont font = value->font();
        font.setBold(true);
        value->setFont(font);

        grid->addWidget(
            label,
            static_cast<int>(i),
            0
        );
        grid->addWidget(
            value,
            static_cast<int>(i),
            1
        );
    }

    network_layout->addLayout(grid);
    layout->addWidget(network);

    auto* safety = make_card();
    auto* safety_layout =
        new QVBoxLayout(safety);
    safety_layout->setContentsMargins(
        22,
        17,
        22,
        19
    );
    safety_layout->setSpacing(11);

    safety_layout->addWidget(
        make_section_heading(
            "Wallet Safety and Recovery",
            "Protect your funds and manage wallet backups."
        )
    );

    auto* recovery_row =
        new QFrame;
    recovery_row->setObjectName(
        "metricCard"
    );
    recovery_row->setProperty(
        "tone",
        "purple"
    );

    auto* recovery_layout =
        new QHBoxLayout(recovery_row);
    recovery_layout->setContentsMargins(
        16,
        12,
        16,
        12
    );

    recovery_layout->addWidget(
        make_section_heading(
            "Show 24 recovery words",
            "View the wallet recovery phrase after password verification."
        ),
        1
    );

    auto* recovery = new QPushButton(
        "Show 24 recovery words"
    );
    recovery->setObjectName(
        "primaryButton"
    );
    recovery_layout->addWidget(recovery);

    connect(
        recovery,
        &QPushButton::clicked,
        this,
        [this] {
            show_recovery_phrase();
        }
    );

    safety_layout->addWidget(
        recovery_row
    );

    auto* backup_row =
        new QFrame;
    backup_row->setObjectName(
        "metricCard"
    );
    backup_row->setProperty(
        "tone",
        "green"
    );

    auto* backup_layout =
        new QHBoxLayout(backup_row);
    backup_layout->setContentsMargins(
        16,
        12,
        16,
        12
    );

    backup_layout->addWidget(
        make_section_heading(
            "Backup complete wallet",
            "Create a .qtmbackup containing wallet.dat and encrypted metadata."
        ),
        1
    );

    auto* backup = new QPushButton(
        "Backup wallet..."
    );
    backup_layout->addWidget(backup);

    connect(
        backup,
        &QPushButton::clicked,
        this,
        [this] {
            backup_wallet_bundle();
        }
    );

    safety_layout->addWidget(
        backup_row
    );
    layout->addWidget(safety);

    auto* advanced = make_card();
    auto* advanced_layout =
        new QVBoxLayout(advanced);
    advanced_layout->setContentsMargins(
        22,
        17,
        22,
        19
    );
    advanced_layout->setSpacing(10);

    advanced_layout->addWidget(
        make_section_heading(
            "Advanced",
            "Consensus and wallet safety values shown here are read-only in this Testnet build."
        )
    );

    auto* advanced_note =
        new QLabel(
            "Network identity, genesis parameters, address format and consensus rules are intentionally not editable from the GUI."
        );
    advanced_note->setObjectName("muted");
    advanced_note->setWordWrap(true);
    advanced_layout->addWidget(
        advanced_note
    );

    layout->addWidget(advanced);
    layout->addStretch();

    return page;
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
        format_amount(balance.available)
    );
    confirmed_value_->setText(
        format_amount(balance.confirmed)
    );
    pending_value_->setText(
        format_amount(balance.pending)
    );
    immature_value_->setText(
        format_amount(balance.immature)
    );

    receive_confirmed_->setText(
        format_amount(balance.confirmed) +
        " QMU"
    );
    receive_pending_->setText(
        format_amount(balance.pending) +
        " QMU"
    );

    Amount received_total{0U};

    for (const auto& view :
         snapshot.transactions) {
        if (view.record.received <=
            std::numeric_limits<Amount>::max() -
                received_total) {
            received_total +=
                view.record.received;
        }
    }

    receive_total_->setText(
        format_amount(received_total) +
        " QMU"
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
        overview_sync_state_->setStyleSheet(
            "color:#d18200;font-weight:700;"
        );
    } else if (status.peer_best_height) {
        overview_sync_state_->setText(
            "Up to date"
        );
        overview_sync_state_->setStyleSheet(
            "color:#079b43;font-weight:700;"
        );
    } else if (status.peers == 0U) {
        overview_sync_state_->setText(
            "Waiting for peers"
        );
        overview_sync_state_->setStyleSheet(
            "color:#c2413b;font-weight:700;"
        );
    } else {
        overview_sync_state_->setText(
            "Connected"
        );
        overview_sync_state_->setStyleSheet(
            "color:#079b43;font-weight:700;"
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
        QString("◉  Network: %1")
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
        "▤  Blocks: " + height
    );
    status_peers_->setText(
        QString("●●  Peers: %1")
            .arg(status.peers)
    );
    QString node_status =
        status.running
            ? "●  Node: running"
            : "●  Node: stopped";

    if (status.running &&
        status.nat_mapping_method !=
            net::NatMappingMethod::none) {
        const QString method =
            status.nat_mapping_method ==
                    net::NatMappingMethod::nat_pmp
                ? "NAT-PMP"
                : "UPnP";

        node_status +=
            QString(" • %1:%2")
                .arg(method)
                .arg(
                    status.nat_external_port
                );
    }

    status_connection_->setText(
        node_status
    );
    status_connection_->setObjectName(
        status.running
            ? "statusRunning"
            : "statusStopped"
    );
    status_connection_->style()
        ->unpolish(status_connection_);
    status_connection_->style()
        ->polish(status_connection_);

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

    send_fee_rate_->setText(
        format_amount(
            status.recommended_fee_rate_per_kb
        ) +
        " QMU/kB"
    );

    if (status.difficulty) {
        const double value =
            *status.difficulty;

        mining_difficulty_->setText(
            value < 1'000'000.0
                ? QString::number(
                      value,
                      'f',
                      value < 10.0 ? 4 : 2
                  )
                : QString::number(
                      value,
                      'g',
                      6
                  )
        );

        if (status.difficulty_bits) {
            mining_difficulty_->setToolTip(
                QString("Active tip bits: 0x%1")
                    .arg(
                        static_cast<qulonglong>(
                            *status.difficulty_bits
                        ),
                        8,
                        16,
                        QChar('0')
                    )
                    .toUpper()
            );
        } else {
            mining_difficulty_->setToolTip({});
        }
    } else {
        mining_difficulty_->setText("—");
        mining_difficulty_->setToolTip({});
    }

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

        auto* status_item =
            new QTableWidgetItem(
                status_text(tx.status)
            );

        if (tx.status ==
            wallet::WalletTransactionStatus::
                confirmed) {
            status_item->setForeground(
                QColor("#079b43")
            );
        } else if (
            tx.status ==
            wallet::WalletTransactionStatus::
                unconfirmed) {
            status_item->setForeground(
                QColor("#d18200")
            );
        } else {
            status_item->setForeground(
                QColor("#6c4ec4")
            );
        }

        transactions_->setItem(
            row,
            0,
            status_item
        );

        const QString full_txid =
            hash_hex(tx.txid);
        const QString short_txid =
            full_txid.size() > 18
                ? full_txid.left(9) +
                    "..." +
                    full_txid.right(7)
                : full_txid;

        auto* txid_item =
            new QTableWidgetItem(
                short_txid
            );
        txid_item->setToolTip(
            full_txid
        );
        txid_item->setForeground(
            QColor("#0869e8")
        );

        transactions_->setItem(
            row,
            1,
            txid_item
        );

        auto* received_item =
            new QTableWidgetItem(
                tx.received > 0U
                    ? "+" +
                        format_amount(
                            tx.received
                        ) +
                        " QMU"
                    : "-"
            );

        if (tx.received > 0U) {
            received_item->setForeground(
                QColor("#079b43")
            );
        }

        transactions_->setItem(
            row,
            2,
            received_item
        );

        transactions_->setItem(
            row,
            3,
            new QTableWidgetItem(
                tx.spent > 0U
                    ? "-" +
                        format_amount(
                            tx.spent
                        ) +
                        " QMU"
                    : "-"
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
                    : (
                          tx.coinbase
                              ? "Mining Reward"
                              : QString{}
                      )
            )
        );
    }

    filter_transactions();

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

void MainWindow::filter_transactions()
{
    if (transactions_ == nullptr ||
        transaction_search_ == nullptr ||
        transaction_status_filter_ ==
            nullptr) {
        return;
    }

    const QString needle =
        transaction_search_
            ->text()
            .trimmed();

    const QString status_filter =
        transaction_status_filter_
            ->currentText();

    for (int row = 0;
         row < transactions_->rowCount();
         ++row) {
        bool text_match =
            needle.isEmpty();

        if (!text_match) {
            for (int column = 0;
                 column <
                    transactions_
                        ->columnCount();
                 ++column) {
                const auto* item =
                    transactions_->item(
                        row,
                        column
                    );

                if (item != nullptr &&
                    item->text().contains(
                        needle,
                        Qt::CaseInsensitive
                    )) {
                    text_match = true;
                    break;
                }

                if (item != nullptr &&
                    item->toolTip().contains(
                        needle,
                        Qt::CaseInsensitive
                    )) {
                    text_match = true;
                    break;
                }
            }
        }

        const auto* status_item =
            transactions_->item(
                row,
                0
            );

        const bool status_match =
            status_filter ==
                "All statuses" ||
            (status_item != nullptr &&
             status_item->text() ==
                 status_filter);

        transactions_->setRowHidden(
            row,
            !(text_match &&
              status_match)
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
        " QMU\nFee: " +
        format_amount(preview.quote.fee) +
        " QMU\nTotal: " +
        format_amount(total) +
        " QMU";

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

    receive_label_->clear();

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

void MainWindow::save_receive_label()
{
    const QString address =
        receive_address_
            ->text()
            .trimmed();

    const QString label =
        receive_label_
            ->text()
            .trimmed();

    if (address.isEmpty() ||
        label.isEmpty()) {
        QMessageBox::warning(
            this,
            "Receive label",
            "Enter a label for the current receive address."
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
            "Label not saved",
            metadata_error_text(result)
        );
        return;
    }

    QMessageBox::information(
        this,
        "Address label saved",
        "The receive address label was saved locally."
    );

    refresh();
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
        mining_state_->setText("Ready");
        mining_hashrate_->setText(
            "0.00 H/s"
        );
        mining_button_->setText(
            "Start mining"
        );
        return;
    }

    mining_total_attempts_ = 0U;
    mining_blocks_found_ = 0U;
    mining_elapsed_.restart();

    if (mining_recent_blocks_ !=
        nullptr) {
        mining_recent_blocks_
            ->setRowCount(0);
    }

    mining_state_->setText("Mining");
    mining_button_->setText(
        "Stop mining"
    );
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

        const Amount reward =
            consensus::block_subsidy(
                result.height
            ) +
            result.total_fees;

        add_recent_mined_block(
            result.height,
            reward
        );
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

    if (mining_blocks_found_ > 0U) {
        const double seconds =
            static_cast<double>(
                elapsed_ms
            ) /
            1000.0 /
            static_cast<double>(
                mining_blocks_found_
            );

        if (seconds < 60.0) {
            mining_eta_->setText(
                QString("%1 s")
                    .arg(
                        seconds,
                        0,
                        'f',
                        1
                    )
            );
        } else {
            mining_eta_->setText(
                QString("%1 min")
                    .arg(
                        seconds / 60.0,
                        0,
                        'f',
                        1
                    )
            );
        }
    } else {
        mining_eta_->setText("—");
    }

    if (mining_timer_->isActive()) {
        mining_state_->setText("Mining");
    }
}

void MainWindow::add_recent_mined_block(
    std::uint32_t height,
    Amount reward)
{
    if (mining_recent_blocks_ ==
        nullptr) {
        return;
    }

    mining_recent_blocks_->insertRow(0);

    mining_recent_blocks_->setItem(
        0,
        0,
        new QTableWidgetItem(
            QString::number(height)
        )
    );
    mining_recent_blocks_->setItem(
        0,
        1,
        new QTableWidgetItem(
            format_amount(reward) +
            " QMU"
        )
    );
    mining_recent_blocks_->setItem(
        0,
        2,
        new QTableWidgetItem(
            QDateTime::currentDateTime()
                .toString("HH:mm:ss")
        )
    );

    while (mining_recent_blocks_
               ->rowCount() > 5) {
        mining_recent_blocks_
            ->removeRow(
                mining_recent_blocks_
                    ->rowCount() - 1
            );
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
