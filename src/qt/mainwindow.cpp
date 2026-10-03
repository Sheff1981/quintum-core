#include "qt/mainwindow.hpp"

#include "consensus/monetary.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
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

} // namespace

MainWindow::MainWindow(
    net::NetworkRuntime& runtime,
    const consensus::ChainParams& params,
    QWidget* parent)
    : QMainWindow(parent),
      runtime_(runtime),
      params_(params)
{
    setWindowTitle("QUINTUM Core");
    resize(1040, 680);

    auto* root = new QWidget;
    auto* layout = new QHBoxLayout(root);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    auto* navigation = new QListWidget;
    navigation->addItems({
        "Overview",
        "Send",
        "Receive",
        "Transactions",
    });
    navigation->setFixedWidth(150);
    navigation->setCurrentRow(0);

    pages_ = new QStackedWidget;
    pages_->addWidget(build_overview_page());
    pages_->addWidget(build_send_page());
    pages_->addWidget(build_receive_page());
    pages_->addWidget(build_transactions_page());

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

    refresh_timer_->start();
    refresh();
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
    overview_peers_ = new QLabel("0");
    overview_mempool_ = new QLabel("0");

    network_layout->addRow("Block height:", overview_height_);
    network_layout->addRow("Peers:", overview_peers_);
    network_layout->addRow("Mempool:", overview_mempool_);

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

    overview_height_->setText(height);
    overview_peers_->setText(
        QString::number(status.peers)
    );
    overview_mempool_->setText(
        QString::number(
            status.mempool_transactions
        )
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
            "Enter a valid QUINTUM address and amount."
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
            "The address, amount, balance or fee is not valid."
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
        const QString error =
            sent.error ==
                    net::NetworkWalletSendError::
                        stale_preview
                ? "The blockchain or mempool changed while you were confirming. Review the payment again."
                : sent.error ==
                          net::NetworkWalletSendError::
                              invalid_preview
                      ? "The payment preview is invalid."
                      : "The transaction was rejected or could not be created.";

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

        (void)runtime_.set_address_label(
            preview.destination,
            label_utf8
        );
        (void)runtime_.set_transaction_label(
            sent.wallet.txid,
            label_utf8
        );
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
            "The wallet could not create a new receive address."
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
