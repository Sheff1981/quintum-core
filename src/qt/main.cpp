#include "qt/mainwindow.hpp"

#include "consensus/chainparams.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <filesystem>
#include <string>
#include <utility>

namespace {

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

QString startup_error_text(
    const quintum::net::NetworkRuntimeStartResult& result)
{
    using quintum::net::NetworkRuntimeStartError;
    using quintum::wallet::WalletStartError;

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
        "Use the QUINTUM test network."
    };
    const QCommandLineOption regtest{
        "regtest",
        "Use local regression-test mode (default)."
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

    parser.addOption(mainnet);
    parser.addOption(testnet);
    parser.addOption(regtest);
    parser.addOption(datadir);
    parser.addOption(smoke_test);
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
        quintum::consensus::Network::regtest;

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

    if (parser.isSet(smoke_test)) {
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
            "stage26-disposable-smoke-wallet";

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

        QTimer::singleShot(
            250,
            &app,
            &QCoreApplication::quit
        );

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

    bool accepted{false};

    QString passphrase =
        QInputDialog::getText(
            nullptr,
            wallet_exists
                ? "Open QUINTUM wallet"
                : "Create QUINTUM wallet",
            wallet_exists
                ? "Wallet password "
                  "(leave blank only for a legacy unencrypted wallet):"
                : "Create a wallet password:",
            QLineEdit::Password,
            {},
            &accepted
        );

    if (!accepted) {
        return 0;
    }

    if (!wallet_exists &&
        passphrase.isEmpty()) {
        QMessageBox::warning(
            nullptr,
            "Password required",
            "New desktop wallets must be encrypted with a password."
        );
        return 5;
    }

    QByteArray passphrase_utf8 =
        passphrase.toUtf8();

    quintum::net::NetworkRuntime runtime{
        params,
        data_path
    };

    quintum::net::NetworkRuntimeConfig config;
    config.wallet_passphrase =
        std::string{
            passphrase_utf8.constData(),
            static_cast<std::size_t>(
                passphrase_utf8.size()
            )
        };

    passphrase.fill(QChar{0});
    passphrase.clear();
    passphrase_utf8.fill('\0');
    passphrase_utf8.clear();

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

    const int result = app.exec();

    runtime.stop();
    return result;
}
