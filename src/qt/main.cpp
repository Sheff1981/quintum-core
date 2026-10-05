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
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
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
    case quintum::consensus::Network::randomx_testnet:
        return "randomx-testnet";
    case quintum::consensus::Network::regtest:
        return "regtest";
    }

    return "unknown";
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

void write_startup_stage(
    const std::filesystem::path& data_path,
    std::string_view stage) noexcept
{
    try {
        std::filesystem::create_directories(
            data_path
        );

        std::ofstream out(
            data_path / "startup.log",
            std::ios::binary |
                std::ios::trunc
        );

        if (out) {
            out << stage << '\n';
            out.flush();
        }
    } catch (...) {
        // Diagnostics must never prevent startup.
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
            NetworkRuntimeStartError::wallet_failed &&
        result.wallet.error ==
            WalletStartError::store_failed) {
        using quintum::wallet::WalletStoreError;

        switch (result.wallet.store_error) {
        case WalletStoreError::invalid_passphrase:
        case WalletStoreError::passphrase_required:
            return
                "The wallet password is incorrect or missing. "
                "No wallet data was changed.";
        case WalletStoreError::wrong_network:
            return
                "This wallet.dat belongs to another QUINTUM network. "
                "No wallet data was changed.";
        case WalletStoreError::corrupt:
            return
                "wallet.dat is corrupt or has been modified. "
                "Do not delete it; restore from a verified backup or recovery words.";
        case WalletStoreError::io_error:
            return
                "wallet.dat could not be read from disk. "
                "Check file access and the data directory. No wallet data was changed.";
        case WalletStoreError::crypto_error:
            return
                "Wallet cryptography could not initialize. "
                "No wallet data was changed.";
        case WalletStoreError::not_found:
        case WalletStoreError::target_exists:
        case WalletStoreError::none:
            break;
        }
    }

    if (result.error ==
            NetworkRuntimeStartError::wallet_failed &&
        result.wallet_sync !=
            quintum::wallet::WalletSyncError::none) {
        return
            "The wallet opened, but its blockchain rescan/synchronization failed. "
            "wallet.dat was not discarded.";
    }

    if (result.error ==
        NetworkRuntimeStartError::wallet_failed) {
        return
            "The wallet could not be opened. "
            "No wallet data was changed.";
    }

    if (result.error ==
        NetworkRuntimeStartError::node_failed) {
        using quintum::NodeStartError;
        using quintum::StorageError;

        if (result.node.error ==
                NodeStartError::storage_failed) {
            switch (result.node.storage_error) {
            case StorageError::wrong_network:
                return
                    "The blockchain data belongs to another QUINTUM network. "
                    "No wallet data was changed.";
            case StorageError::checksum_mismatch:
            case StorageError::truncated:
            case StorageError::bad_format:
            case StorageError::unsupported_version:
            case StorageError::state_mismatch:
            case StorageError::consensus_replay_failed:
                return
                    "The local blockchain database could not be validated. "
                    "Do not delete wallet.dat; blockchain data can be resynchronized.";
            case StorageError::io_error:
                return
                    "The local blockchain database could not be read or written. "
                    "Check disk access and free space. wallet.dat was not changed.";
            case StorageError::not_found:
            case StorageError::none:
                break;
            }
        }

        return
            "The local QUINTUM blockchain could not start safely. "
            "wallet.dat was not changed.";
    }

    if (result.error ==
        NetworkRuntimeStartError::address_store_failed) {
        return
            "The peer database (peers.dat) could not be loaded or saved. "
            "The wallet and blockchain were not discarded.";
    }

    if (result.error ==
        NetworkRuntimeStartError::listener_failed) {
        return
            "The QUINTUM P2P listener could not start, even after the safe alternate-port retry. "
            "Check local socket/network policy. The wallet was not changed.";
    }

    if (result.error ==
        NetworkRuntimeStartError::worker_start_failed) {
        return
            "QUINTUM could not start its background network worker. "
            "The wallet was opened safely and no wallet data was changed.";
    }

    return
        "QUINTUM Core could not start its node runtime. "
        "No wallet data was changed.";
}

} // namespace

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // Remote Desktop / Windows Server sessions can expose incomplete or
    // unstable hardware graphics stacks. QUINTUM is a QWidget application,
    // so prefer the software backend for maximum server compatibility.
    qputenv("QT_OPENGL", "software");
#endif

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
        "Use the QUINTUM test network."
    };
    const QCommandLineOption randomx_testnet{
        "randomx-testnet",
        "Use the QUINTUM RandomX public test network."
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
    parser.addOption(randomx_testnet);
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
            parser.isSet(randomx_testnet)
        ) +
        static_cast<int>(
            parser.isSet(regtest)
        );

    if (selected_networks > 1) {
        QMessageBox::critical(
            nullptr,
            "Invalid network selection",
            "Choose only one of --mainnet, --testnet, --randomx-testnet, or --regtest."
        );
        return 2;
    }

    quintum::consensus::Network network =
        quintum::consensus::Network::randomx_testnet;

    if (parser.isSet(mainnet)) {
        network =
            quintum::consensus::Network::mainnet;
    } else if (parser.isSet(testnet)) {
        network =
            quintum::consensus::Network::testnet;
    } else if (parser.isSet(randomx_testnet)) {
        network =
            quintum::consensus::Network::randomx_testnet;
    } else if (parser.isSet(regtest)) {
        network =
            quintum::consensus::Network::regtest;
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

        data_directory =
            QDir(root).filePath(
                network_directory_name(
                    network
                )
            );
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

    write_startup_stage(
        data_path,
        "wallet_setup_complete"
    );

    QProgressDialog startup_progress;
    startup_progress.setWindowTitle(
        "Starting QUINTUM Core"
    );
    startup_progress.setLabelText(
        "Opening wallet and starting the node..."
    );
    startup_progress.setCancelButton(nullptr);
    startup_progress.setRange(0, 0);
    startup_progress.setMinimumDuration(0);
    startup_progress.setAutoClose(false);
    startup_progress.show();
    QApplication::processEvents();

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
    config.allow_ephemeral_listener_fallback =
        true;

    wipe_string(setup.password);
    wipe_string(setup.mnemonic);

    quintum::net::NetworkRuntimeStartResult started;

    write_startup_stage(
        data_path,
        "runtime_start_begin"
    );

    try {
        started =
            runtime.start(
                std::move(config)
            );
    } catch (const std::exception& error) {
        QMessageBox::critical(
            nullptr,
            "QUINTUM Core did not start",
            QString(
                "QUINTUM caught an unexpected startup error instead of closing silently.\n\n%1\n\nNo wallet data was intentionally discarded."
            ).arg(
                QString::fromUtf8(
                    error.what()
                )
            )
        );
        runtime.stop();
        return 6;
    } catch (...) {
        QMessageBox::critical(
            nullptr,
            "QUINTUM Core did not start",
            "QUINTUM caught an unexpected startup error instead of closing silently. "
            "No wallet data was intentionally discarded."
        );
        runtime.stop();
        return 6;
    }

    if (!started.ok()) {
        QMessageBox::critical(
            nullptr,
            "QUINTUM Core did not start",
            startup_error_text(started)
        );
        runtime.stop();
        return 6;
    }

    write_startup_stage(
        data_path,
        "runtime_start_ok"
    );

    startup_progress.setLabelText(
        "Opening QUINTUM interface..."
    );
    QApplication::processEvents();

    std::unique_ptr<quintum::qtui::MainWindow>
        window;

    try {
        write_startup_stage(
            data_path,
            "mainwindow_construct_begin"
        );

        window =
            std::make_unique<
                quintum::qtui::MainWindow>(
                    runtime,
                    params
                );

        write_startup_stage(
            data_path,
            "mainwindow_construct_ok"
        );

        window->show();
        window->raise();
        window->activateWindow();
        QApplication::processEvents();

        write_startup_stage(
            data_path,
            "ready"
        );
    } catch (const std::exception& error) {
        startup_progress.close();
        QMessageBox::critical(
            nullptr,
            "QUINTUM interface could not start",
            QString(
                "The node and wallet started, but the desktop interface failed to open.\n\n%1\n\nNo wallet data was intentionally discarded."
            ).arg(
                QString::fromUtf8(
                    error.what()
                )
            )
        );
        runtime.stop();
        return 8;
    } catch (...) {
        startup_progress.close();
        QMessageBox::critical(
            nullptr,
            "QUINTUM interface could not start",
            "The node and wallet started, but the desktop interface failed to open. "
            "No wallet data was intentionally discarded."
        );
        runtime.stop();
        return 8;
    }

    startup_progress.close();

    const auto live_status =
        runtime.status();

    if (live_status.listen_port !=
            params.p2p_port &&
        live_status.listen_port != 0U) {
        QTimer::singleShot(
            0,
            window.get(),
            [parent = window.get(),
             actual_port = live_status.listen_port,
             expected_port = params.p2p_port] {
                QMessageBox::warning(
                    parent,
                    "P2P port fallback",
                    QString(
                        "The default P2P port %1 was unavailable. "
                        "QUINTUM stayed running and switched to local port %2. "
                        "Outbound peers, synchronization and block relay still work."
                    )
                        .arg(expected_port)
                        .arg(actual_port)
                );
            }
        );
    }

    if (started.recovered_wallet) {
        QMessageBox::information(
            window.get(),
            "Wallet recovered",
            "The wallet was recovered from the 24 words and rescanned successfully."
        );
    }

    const int result = app.exec();

    runtime.stop();
    return result;
}
