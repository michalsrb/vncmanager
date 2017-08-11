/*
 * Copyright (c) 2016 Michal Srb <michalsrb@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>

#include <boost/tokenizer.hpp>

#include "helper.h"
#include "rfb.h"
#include "Configuration.h"
#include "Log.h"


namespace po = boost::program_options;


typedef std::vector<VeNCryptSubtype> VeNCryptSubtypesList;

static void validate(boost::any &v, std::vector<std::string> const &tokens, VeNCryptSubtypesList *target_type, int /* boost template trickery */)
{
    if (v.empty())
        v = boost::any(VeNCryptSubtypesList());

    VeNCryptSubtypesList *p = boost::any_cast<VeNCryptSubtypesList>(&v);

    boost::char_separator<char> sep(",");
    for (const std::string & token : tokens) {
        boost::tokenizer<boost::char_separator<char>> tok(token, sep);
        for (auto str : tok) {
            if (str == "TLS")       p->push_back(VeNCryptSubtype::TLSNone);
            else if (str == "X509") p->push_back(VeNCryptSubtype::X509None);
            else if (str == "None") p->push_back(VeNCryptSubtype::None);
            else throw po::invalid_option_value("Unknown security type: " + str);
        }
    }

    if (p->empty())
        throw po::invalid_option_value("No security type configured.");
}

static void validate(boost::any &v, std::vector<std::string> const &tokens, XvncArgList *target_type, int /* boost template trickery */)
{
    if (v.empty())
        v = boost::any(XvncArgList());

    XvncArgList *p = boost::any_cast<XvncArgList>(&v);

    boost::escaped_list_separator<char> sep('\\', ' ', '\"');
    for (const std::string & token : tokens) {
        boost::tokenizer<boost::escaped_list_separator<char>> tok(token, sep);
        for (auto str : tok) {
            p->values.push_back(str);
        }
    }
}

bool Configuration::parse(int argc, char *argv[], const char *config)
{
    VeNCryptSubtypesList defaultSecurity = { VeNCryptSubtype::TLSNone, VeNCryptSubtype::X509None, VeNCryptSubtype::None };
    std::string defaultSecurityTxt = "TLS,X509,None";

    po::options_description all;
    all.add_options()
        ("help", "produce help message")

        ("config",   po::value<std::string>()->default_value(config), "configuration file");

    po::options_description general("General");
    general.add_options()
        ("listen", po::value<std::vector<std::string>>()->multitoken(), "addresses to bind to")
        ("port",   po::value<std::string>()->default_value("5900"), "tcp port to listen on")

        ("security", po::value<VeNCryptSubtypesList>()->default_value(defaultSecurity, defaultSecurityTxt), "list of VNC security types separated by commas, ordered by priority")

        ("disable-manager",     po::value<bool>()->default_value(false, "no"), "If set, every connection will receive unique session, not sharing or reconnection possible.")
        ("always-show-greeter", po::value<bool>()->default_value(false, "no"), "If set, greeter will be shown even when there are no session available for reconnection.")

        ("query", po::value<std::string>()->default_value("localhost"), "Address of XDMCP server that Xvnc should query.")

        ("geometry", po::value<std::string>()->default_value("1024x768"), "<width>x<height> The value of geometry parameter given to Xvnc. Sets the initial resolution.")

        ("xvnc",    po::value<std::string>()->default_value("/usr/bin/Xvnc"),               "path to Xvnc executable")
        ("greeter", po::value<std::string>()->default_value("/usr/bin/vncmanager-greeter"), "path to Greeter executable")
        ("xauth",   po::value<std::string>()->default_value("/usr/bin/xauth"),              "path to xauth executable")
        ("rundir",  po::value<std::string>()->default_value("/run/vncmanager"),             "path to run directory")

        ("xvnc-args", po::value<XvncArgList>()->multitoken(), "Additional arguments that will be passed to Xvnc. Take care to not overwrite arguments set by vncmanager.");

    po::options_description tls("TLS");
    tls.add_options()
        ("tls-cert",                 po::value<std::string>()->default_value("/etc/vnc/tls.cert"),          "path to certificate file")
        ("tls-key",                  po::value<std::string>()->default_value("/etc/vnc/tls.key"),           "path to key file")
        ("tls-priority-anonymous",   po::value<std::string>()->default_value("NORMAL:+ANON-ECDH:+ANON-DH"), "GNUTLS priority configuration for anonymous TLS")         // TODO: Verify the default value
        ("tls-priority-certificate", po::value<std::string>()->default_value("NORMAL"),                     "GNUTLS priority configuration for TLS with certificate"); // TODO: Verify the default value

    all.add(general).add(tls);

    po::store(po::parse_command_line(argc, argv, all), options);
    std::ifstream config_file(options["config"].as<std::string>());
    if (config_file.good()) {
        po::store(po::parse_config_file<char>(config_file, all), options);
    } else {
        if (!options["config"].defaulted()) {
            Log::error() << "Failed to read configuration file " << options["config"].as<std::string>() << std::endl;
            return false;
        }
    }
    po::notify(options);

    if (options.count("help") > 0) {
        std::cout << all << "\n";
        return false;
    }

    check();

    return true;
}

void Configuration::check()
{
    std::string xvnc = options["xvnc"].as<std::string>();
    if(access(xvnc.c_str(), X_OK) < 0)
        throw_errno(xvnc);

    // greeter and xauth are not needed if the manager is disabled
    if(!options["disable-manager"].as<bool>()) {
        std::string greeter = options["greeter"].as<std::string>();
        if(access(greeter.c_str(), X_OK) < 0)
            throw_errno(greeter);

        std::string xauth = options["xauth"].as<std::string>();
        if(access(xauth.c_str(), X_OK) < 0)
            throw_errno(xauth);
    }

    // If X509 is selected, then key and certificate must be in place
    VeNCryptSubtypesList security = options["security"].as<VeNCryptSubtypesList>();
    if(std::find(security.begin(), security.end(), VeNCryptSubtype::X509None) != security.end()) {
        std::string tls_cert = options["tls-cert"].as<std::string>();
        if(access(tls_cert.c_str(), R_OK) < 0)
            throw_errno(tls_cert);

        std::string tls_key = options["tls-key"].as<std::string>();
        if(access(tls_key.c_str(), R_OK) < 0)
            throw_errno(tls_key);
    }
}

boost::program_options::variables_map Configuration::options;
