#pragma once

#include "utils/log-wrapper.hpp"
#include "utils/mounter.hpp"
#include "utils/utils.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

class SmbShare
{
  public:
    SmbShare(const std::filesystem::path& mountDir) : mountDir(mountDir) {}

    std::unique_ptr<utils::Mounter>
        mount(const std::filesystem::path& remote, bool rw,
              const std::unique_ptr<utils::CredentialsProvider>& credentials)
    {
        LOGGER_DEBUG << "Trying to mount remote : " << remote;

        const std::string params = "sec=ntlmsspi,seal";
        const std::string perm = rw ? "rw" : "ro";
        std::string options = params + "," + perm;
        std::string credentialsOpt;

        if (!credentials)
        {
            LOGGER_INFO << "Mounting as Guest";

            credentialsOpt = "guest,username=OpenBmc";
        }
        else
        {
            if (!validateUsername(credentials->user()))
            {
                LOGGER_ERROR
                    << "Username for CIFS share can't contain ',' character";

                return nullptr;
            }
            credentials->escapeCommas();
            credentialsOpt = "username=" + credentials->user() +
                             ",password=" + credentials->password();
        }
        options += "," + credentialsOpt;

        std::string versionOpt = "vers=3.1.1";
        auto mounter = mountWithSmbVers(remote, options, versionOpt);

        if (!mounter)
        {
            // vers=3 will negotiate max version from 3.02 and 3.0
            versionOpt = "vers=3";
            mounter = mountWithSmbVers(remote, options, versionOpt);
        }

        utils::secureCleanup(options);
        utils::secureCleanup(credentialsOpt);

        return mounter;
    }

  private:
    std::string mountDir;
    /* Check if username does not contain comma (,) character */
    bool validateUsername(const std::string& username)
    {
        return username.find(',') == std::string::npos;
    }

    std::unique_ptr<utils::Mounter>
        mountWithSmbVers(const std::filesystem::path& remote,
                         std::string options, const std::string& version)
    {
        std::unique_ptr<utils::Mounter> mounter = nullptr;
        options += "," + version;

        try
        {
            mounter = std::make_unique<utils::Mounter>(remote, mountDir, "cifs",
                                                       0, options.c_str());
        }
        catch (const std::system_error& e)
        {
            LOGGER_INFO << "Mount failed for " << version << ": " << e.what();

            mounter = nullptr;
        }

        utils::secureCleanup(options);
        return mounter;
    }
};
