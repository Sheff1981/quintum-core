#include "qt/mainwindow.hpp"

#include "consensus/chainparams.hpp"
#include "crypto/random.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRadioButton>
#include <QSettings>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace {

struct WalletSetup {
    bool accepted{false};
    std::string password{};
    std::string mnemonic{};
    std::filesystem::path restore_bundle{};
};

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

QString network_directory_name(
    quintum::consensus::Network network)
{
    switch (network) {
    case quintum::consensus::Network::mainnet:
        return "mainnet";
    case quintum::consensus::Network::testnet:
        return "testnet";
    case quintum::consensus::Network::regtest:
        return "regtest";
    }

    return "unknown";
}

std::optional<QString> choose_data_directory(
    QWidget* parent,
    const QString& default_directory)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(
        "Welcome to QUINTUM Core"
    );
    dialog.setMinimumWidth(610);

    auto* layout = new QVBoxLayout(&dialog);

    auto* intro = new QLabel(
        "This is the first time QUINTUM Core is being started. "
        "Choose where blockchain and wallet data will be stored. "
        "You can use the default location or select another directory."
    );
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* default_option =
        new QRadioButton(
            "Use the default data directory"
        );
    auto* custom_option =
        new QRadioButton(
            "Use a custom data directory:"
        );
    default_option->setChecked(true);

    layout->addWidget(default_option);
    layout->addWidget(custom_option);

    auto* path_row = new QHBoxLayout;
    auto* path = new QLineEdit(
        default_directory
    );
    auto* browse = new QPushButton("...");
    path->setEnabled(false);
    browse->setEnabled(false);

    path_row->addWidget(path, 1);
    path_row->addWidget(browse);
    layout->addLayout(path_row);

    auto update_mode = [=] {
        const bool custom =
            custom_option->isChecked();
        path->setEnabled(custom);
        browse->setEnabled(custom);

        if (!custom) {
            path->setText(
                default_directory
            );
        }
    };

    QObject::connect(
        default_option,
        &QRadioButton::toggled,
        &dialog,
        update_mode
    );
    QObject::connect(
        custom_option,
        &QRadioButton::toggled,
        &dialog,
        update_mode
    );

    QObject::connect(
        browse,
        &QPushButton::clicked,
        &dialog,
        [=] {
            const QString selected =
                QFileDialog::
                    getExistingDirectory(
                        &dialog,
                        "Choose QUINTUM data directory",
                        path->text()
                    );

            if (!selected.isEmpty()) {
                path->setText(
                    QDir::cleanPath(
                        selected
                    )
                );
            }
        }
    );

    auto* note = new QLabel(
        "QUINTUM Core will keep its blockchain, peer database and wallet data here. "
        "Wallet data is preserved when the application is updated or uninstalled."
    );
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Ok |
            QDialogButtonBox::Cancel
        );
    layout->addWidget(buttons);

    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept
    );
    QObject::connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
    );

    if (dialog.exec() !=
        QDialog::Accepted) {
        return std::nullopt;
    }

    const QString selected =
        QDir::cleanPath(
            path->text().trimmed()
        );

    if (selected.isEmpty()) {
        return std::nullopt;
    }

    return selected;
}

void wipe_byte_array(QByteArray& bytes)
{
    bytes.fill('\0');
    bytes.clear();
    bytes.squeeze();
}

void wipe_string(std::string& value)
{
    if (!value.empty()) {
        quintum::crypto::secure_erase(
            std::span<quintum::Byte>{
                reinterpret_cast<quintum::Byte*>(
                    value.data()),
                value.size()
            }
        );
        value.clear();
    }
}

WalletSetup request_wallet_setup(
    QWidget* parent,
    bool recover)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(
        recover
            ? "Recover QUINTUM wallet"
            : "Create QUINTUM wallet"
    );
    dialog.setMinimumWidth(520);

    auto* layout = new QVBoxLayout(&dialog);

    QTextEdit* mnemonic{nullptr};

    if (recover) {
        auto* warning = new QLabel(
            "Enter exactly the 24 recovery words in order. "
            "Recovery will rescan the active blockchain."
        );
        warning->setWordWrap(true);
        layout->addWidget(warning);

        mnemonic = new QTextEdit;
        mnemonic->setPlaceholderText(
            "word1 word2 ... word24"
        );
        mnemonic->setAcceptRichText(false);
        mnemonic->setTabChangesFocus(true);
        layout->addWidget(mnemonic);
    }

    auto* form = new QFormLayout;
    auto* password = new QLineEdit;
    auto* confirmation = new QLineEdit;

    password->setEchoMode(QLineEdit::Password);
    confirmation->setEchoMode(QLineEdit::Password);

    form->addRow("New wallet password:", password);
    form->addRow("Confirm password:", confirmation);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok |
        QDialogButtonBox::Cancel
    );
    layout->addWidget(buttons);

    QObject::connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
    );

    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        [&] {
            if (password->text().isEmpty()) {
                QMessageBox::warning(
                    &dialog,
                    "Password required",
                    "Desktop wallets must be encrypted with a password."
                );
                return;
            }

            if (password->text() !=
                confirmation->text()) {
                QMessageBox::warning(
                    &dialog,
                    "Passwords do not match",
                    "Enter the same password twice."
                );
                return;
            }

            if (recover &&
                (mnemonic == nullptr ||
                 mnemonic->toPlainText()
                     .simplified()
                     .isEmpty())) {
                QMessageBox::warning(
                    &dialog,
                    "Recovery words required",
                    "Enter the 24 recovery words."
                );
                return;
            }

            dialog.accept();
        }
    );

    WalletSetup out;

    if (dialog.exec() != QDialog::Accepted) {
        password->clear();
        confirmation->clear();

        if (mnemonic != nullptr) {
            mnemonic->clear();
        }

        return out;
    }

    QByteArray password_utf8 =
        password->text().toUtf8();

    out.password.assign(
        password_utf8.constData(),
        static_cast<std::size_t>(
            password_utf8.size()
        )
    );

    if (recover && mnemonic != nullptr) {
        QByteArray words_utf8 =
            mnemonic->toPlainText()
                .simplified()
                .toUtf8();

        out.mnemonic.assign(
            words_utf8.constData(),
            static_cast<std::size_t>(
                words_utf8.size()
            )
        );

        wipe_byte_array(words_utf8);
        mnemonic->clear();
    }

    wipe_byte_array(password_utf8);
    password->clear();
    confirmation->clear();

    out.accepted = true;
    return out;
}

QString recovery_error_text(
    const quintum::net::NetworkRuntimeStartResult& result)
{
    using quintum::wallet::MnemonicError;
    using quintum::wallet::WalletRecoveryError;

    switch (result.wallet_recovery.error) {
    case WalletRecoveryError::invalid_mnemonic:
        switch (result.wallet_recovery.mnemonic_error) {
        case MnemonicError::wrong_word_count:
            return "Recovery requires exactly 24 words.";
        case MnemonicError::unknown_word:
            return "One or more recovery words are not recognized.";
        case MnemonicError::invalid_checksum:
            return "The 24 recovery words have an invalid checksum or order.";
        default:
            return "The recovery phrase is invalid.";
        }
    case WalletRecoveryError::target_exists:
        return "wallet.dat already exists. QUINTUM will not overwrite an existing wallet.";
    case WalletRecoveryError::invalid_passphrase:
        return "The new wallet password is not valid.";
    case WalletRecoveryError::chain_not_ready:
        return "The blockchain is not ready for wallet recovery.";
    case WalletRecoveryError::derivation_failed:
    case WalletRecoveryError::key_limit:
        return "Recovery key derivation failed.";
    case WalletRecoveryError::store_failed:
        return "Recovered keys could not be saved safely.";
    case WalletRecoveryError::sync_failed:
        return "The wallet could not complete its recovery rescan.";
    case WalletRecoveryError::already_started:
    case WalletRecoveryError::invalid_gap_limit:
        return "Wallet recovery could not be started safely.";
    case WalletRecoveryError::none:
        break;
    }

    return {};
}

QString store_error_text(
    quintum::wallet::WalletStoreError error)
{
    using quintum::wallet::WalletStoreError;

    switch (error) {
    case WalletStoreError::none:
        return {};
    case WalletStoreError::not_found:
        return "The backup file was not found.";
    case WalletStoreError::io_error:
        return "The backup could not be read or restored safely.";
    case WalletStoreError::corrupt:
        return "The backup file is corrupt or has been modified.";
    case WalletStoreError::wrong_network:
        return "The backup belongs to another QUINTUM network.";
    case WalletStoreError::target_exists:
        return "wallet.dat already exists. QUINTUM will not overwrite it.";
    case WalletStoreError::passphrase_required:
    case WalletStoreError::invalid_passphrase:
        return "The wallet password is missing or invalid.";
    case WalletStoreError::crypto_error:
        return "The wallet cryptography operation failed.";
    }

    return "The wallet operation failed.";
}

QString startup_error_text(
    const quintum::net::NetworkRuntimeStartResult& result)
{
    using quintum::net::NetworkRuntimeStartError;
    using quintum::wallet::WalletStartError;

    const QString recovery =
        recovery_error_text(result);

    if (!recovery.isEmpty()) {
        return recovery;
    }

    if (result.error ==
            NetworkRuntimeStartError::wallet_failed &&
        result.wallet.error ==
            WalletStartError::metadata_failed) {
        return
            "Wallet metadata could not be loaded. "
            "The file may be corrupt, from another network, "
            "or from another wallet. No metadata was discarded.";
    }

    if (result.error ==
        NetworkRuntimeStartError::wallet_failed) {
        return
            "The wallet could not be opened. "
            "Check the wallet password and data directory.";
    }

    if (result.error ==
        NetworkRuntimeStartError::listener_failed) {
        return
            "The QUINTUM P2P listener could not start. "
            "The configured port may already be in use.";
    }

    return
        "QUINTUM Core could not start its node runtime.";
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(
        "QUINTUM"
    );
    QCoreApplication::setApplicationName(
        "QUINTUM Core"
    );
    QCoreApplication::setApplicationVersion(
        "0.0.1-prealpha"
    );

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "QUINTUM Core desktop wallet"
    );
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption mainnet{
        "mainnet",
        "Use the pre-mainnet candidate network."
    };
    const QCommandLineOption testnet{
        "testnet",
        "Use the QUINTUM public test network (default)."
    };
    const QCommandLineOption regtest{
        "regtest",
        "Use local regression-test mode."
    };
    const QCommandLineOption datadir{
        "datadir",
        "Use an explicit data directory.",
        "path"
    };
    const QCommandLineOption smoke_test{
        "smoke-test",
        "Run an automated desktop smoke test."
    };
    const QCommandLineOption installer_hold_test{
        "installer-hold-test",
        "Run a disposable wallet and remain open for installer update testing."
    };

    parser.addOption(mainnet);
    parser.addOption(testnet);
    parser.addOption(regtest);
    parser.addOption(datadir);
    parser.addOption(smoke_test);
    parser.addOption(installer_hold_test);
    parser.process(app);

    const int selected_networks =
        static_cast<int>(
            parser.isSet(mainnet)
        ) +
        static_cast<int>(
            parser.isSet(testnet)
        ) +
        static_cast<int>(
            parser.isSet(regtest)
        );

    if (selected_networks > 1) {
        QMessageBox::critical(
            nullptr,
            "Invalid network selection",
            "Choose only one of --mainnet, --testnet, or --regtest."
        );
        return 2;
    }

    quintum::consensus::Network network =
        quintum::consensus::Network::testnet;

    if (parser.isSet(mainnet)) {
        network =
            quintum::consensus::Network::mainnet;
    } else if (parser.isSet(testnet)) {
        network =
            quintum::consensus::Network::testnet;
    }

    const auto& params =
        quintum::consensus::chain_params(
            network
        );

    if (parser.isSet(smoke_test) ||
        parser.isSet(installer_hold_test)) {
        QTemporaryDir temporary;

        if (!temporary.isValid()) {
            return 20;
        }

        const auto& smoke_params =
            quintum::consensus::regtest_params();

        quintum::net::NetworkRuntime smoke_runtime{
            smoke_params,
            filesystem_path(
                temporary.path()
            )
        };

        quintum::net::NetworkRuntimeConfig smoke_config;
        smoke_config.listen_port = 0U;
        smoke_config.target_outbound = 0U;
        smoke_config.wallet_passphrase =
            "stage27-disposable-smoke-wallet";

        const auto smoke_started =
            smoke_runtime.start(
                std::move(smoke_config)
            );

        if (!smoke_started.ok()) {
            smoke_runtime.stop();
            return 21;
        }

        quintum::qtui::MainWindow smoke_window{
            smoke_runtime,
            smoke_params
        };
        smoke_window.show();

        if (parser.isSet(smoke_test)) {
            QTimer::singleShot(
                250,
                &app,
                &QCoreApplication::quit
            );
        }

        const int smoke_result =
            app.exec();

        smoke_runtime.stop();
        return smoke_result;
    }

    QString data_directory;

    if (parser.isSet(datadir)) {
        data_directory =
            QDir::cleanPath(
                parser.value(datadir)
            );
    } else {
        const QString root =
            QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation
            );

        const QString default_directory =
            QDir(root).filePath(
                network_directory_name(
                    network
                )
            );

        QSettings settings;
        const QString settings_key =
            "dataDirectory/" +
            network_directory_name(
                network
            );

        if (settings.contains(
                settings_key)) {
            data_directory =
                QDir::cleanPath(
                    settings.value(
                        settings_key
                    ).toString()
                );
        } else {
            const auto selected =
                choose_data_directory(
                    nullptr,
                    default_directory
                );

            if (!selected) {
                return 0;
            }

            data_directory = *selected;
            settings.setValue(
                settings_key,
                data_directory
            );
        }
    }

    if (data_directory.isEmpty() ||
        !QDir{}.mkpath(data_directory)) {
        QMessageBox::critical(
            nullptr,
            "Data directory error",
            "QUINTUM could not create its data directory."
        );
        return 3;
    }

    const std::filesystem::path data_path =
        filesystem_path(
            data_directory
        );

    std::error_code ec;
    const bool wallet_exists =
        std::filesystem::exists(
            data_path / "wallet.dat",
            ec
        );

    if (ec) {
        QMessageBox::critical(
            nullptr,
            "Wallet error",
            "QUINTUM could not inspect wallet.dat."
        );
        return 4;
    }

    WalletSetup setup;

    if (wallet_exists) {
        bool accepted{false};

        QString passphrase =
            QInputDialog::getText(
                nullptr,
                "Open QUINTUM wallet",
                "Wallet password "
                "(leave blank only for a legacy unencrypted wallet):",
                QLineEdit::Password,
                {},
                &accepted
            );

        if (!accepted) {
            return 0;
        }

        QByteArray password_utf8 =
            passphrase.toUtf8();

        setup.password.assign(
            password_utf8.constData(),
            static_cast<std::size_t>(
                password_utf8.size()
            )
        );

        wipe_byte_array(password_utf8);
        passphrase.fill(QChar{0});
        passphrase.clear();
        setup.accepted = true;
    } else {
        QMessageBox chooser;
        chooser.setWindowTitle(
            "Set up QUINTUM wallet"
        );
        chooser.setText(
            "Create a new encrypted wallet, recover from 24 words, or restore a complete QUINTUM backup."
        );

        auto* create_button =
            chooser.addButton(
                "Create new wallet",
                QMessageBox::AcceptRole
            );
        auto* recover_button =
            chooser.addButton(
                "Recover from 24 words",
                QMessageBox::ActionRole
            );
        auto* restore_button =
            chooser.addButton(
                "Restore backup",
                QMessageBox::ActionRole
            );
        chooser.addButton(
            QMessageBox::Cancel
        );
        chooser.exec();

        if (chooser.clickedButton() ==
            create_button) {
            setup =
                request_wallet_setup(
                    nullptr,
                    false
                );
        } else if (chooser.clickedButton() ==
                   recover_button) {
            setup =
                request_wallet_setup(
                    nullptr,
                    true
                );
        } else if (chooser.clickedButton() ==
                   restore_button) {
            const QString selected =
                QFileDialog::getOpenFileName(
                    nullptr,
                    "Restore QUINTUM backup",
                    {},
                    "QUINTUM backup (*.qtmbackup);;All files (*)"
                );

            if (selected.isEmpty()) {
                return 0;
            }

            bool accepted{false};
            QString passphrase =
                QInputDialog::getText(
                    nullptr,
                    "Open restored QUINTUM wallet",
                    "Wallet password from this backup:",
                    QLineEdit::Password,
                    {},
                    &accepted
                );

            if (!accepted) {
                return 0;
            }

            QByteArray password_utf8 =
                passphrase.toUtf8();

            setup.password.assign(
                password_utf8.constData(),
                static_cast<std::size_t>(
                    password_utf8.size()
                )
            );

            setup.restore_bundle =
                filesystem_path(selected);

            wipe_byte_array(password_utf8);
            passphrase.fill(QChar{0});
            passphrase.clear();
            setup.accepted = true;
        } else {
            return 0;
        }
    }

    if (!setup.accepted) {
        return 0;
    }

    quintum::net::NetworkRuntime runtime{
        params,
        data_path
    };

    if (!setup.restore_bundle.empty()) {
        const auto restored =
            runtime.restore_wallet_bundle(
                setup.restore_bundle
            );

        if (restored !=
            quintum::wallet::WalletStoreError::none) {
            QMessageBox::critical(
                nullptr,
                "Backup restore failed",
                store_error_text(restored)
            );
            wipe_string(setup.password);
            wipe_string(setup.mnemonic);
            return 7;
        }
    }

    quintum::net::NetworkRuntimeConfig config;
    config.wallet_passphrase =
        setup.password;
    config.wallet_recovery_mnemonic =
        setup.mnemonic;

    wipe_string(setup.password);
    wipe_string(setup.mnemonic);

    const auto started =
        runtime.start(
            std::move(config)
        );

    if (!started.ok()) {
        QMessageBox::critical(
            nullptr,
            "QUINTUM Core did not start",
            startup_error_text(started)
        );
        runtime.stop();
        return 6;
    }

    quintum::qtui::MainWindow window{
        runtime,
        params
    };
    window.show();

    if (started.recovered_wallet) {
        QMessageBox::information(
            &window,
            "Wallet recovered",
            "The wallet was recovered from the 24 words and rescanned successfully."
        );
    }

    const int result = app.exec();

    runtime.stop();
    return result;
}
