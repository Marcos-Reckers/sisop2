#include "clientClass.h"


int main(int argc, char const *argv[])
{
    if (argc < 4)
    {
        std::cerr << "Uso: " << argv[0] << "<username> <hostname> <port>" << std::endl;
        return 1;
    }

    std::vector<std::string> args{argv, argv + argc};

    struct hostent *server;

    server = gethostbyname(argv[2]);
    if (server == NULL)
    {
        std::cerr << "ERROR, no such host" << std::endl;
        exit(0);
    }

    Client client(args[1], server, args[3]);

    int sock = client.connect_to_server();
    if (sock < 0)
    {
        std::cerr << "Erro ao conectar ao servidor" << std::endl;
        std::cerr << "Erro: " << sock << std::endl;
        return 1;
    }
    else
    {
        std::cout << "Conectado ao servidor" << std::endl;
        // Corrigir a criação das threads especificando o tipo da função membro
        std::thread send_sync_thread(&Client::handle_sync, &client, sock);
        std::thread receive_sync_thread(&Client::handle_sync_request, &client, sock);
        while (true)
        {
            std::string cmd;
            std::cout << "Digite um comando: " << std::flush;
            std::getline(std::cin, cmd);

            if (cmd.rfind("upload", 0) == 0)
            {
                std::string file_path = cmd.substr(7);
                if (!file_path.empty())
                {
                    cout << "Enviando arquivo: " << file_path << endl;
                    if (std::filesystem::exists(file_path))
                    {
                        FileInfo::send_cmd("upload", sock);
                        FileInfo::send_file(file_path, sock);
                    }
                    else
                    {
                        std::cerr << "UPLOAD: Arquivo não encontrado." << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Comando inválido. Uso: upload <path/filename.ext>" << std::endl;
                }
            }
            else if (cmd.rfind("delete", 0) == 0)
            {
                std::string file_name = cmd.substr(7);
                if (!file_name.empty())
                {
                    FileInfo::send_cmd("delete", sock);
                    FileInfo::send_file_name(file_name, sock);
                }
                else
                {
                    std::cerr << "Comando inválido. Uso: delete <filename.ext>" << std::endl;
                }
            }
            else if (cmd.rfind("download", 0) == 0)
            {
                std::string file_name = cmd.substr(9);
                if (!file_name.empty())
                {
                    FileInfo::send_cmd("download", sock);
                    FileInfo::send_file_name(file_name, sock);
                    // Trocar a chamada atual por:
                    string received_file = FileInfo::receive_file("/downloads", sock);
                    if (!received_file.empty()) {
                        cout << "Arquivo " << received_file << " baixado com sucesso" << endl;
                    }
                }
                else
                {
                    std::cerr << "Comando inválido. Uso: download <filename.ext>" << std::endl;
                }
            }
            else if (cmd.rfind("list_server", 0) == 0)
            {
                FileInfo::send_cmd("list_server", sock);
                FileInfo::print_list_files(FileInfo::receive_list_files(sock));
                
            }
            else if (cmd.rfind("list_client", 0) == 0)
            {
                std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
                std::string path = exec_path + "/sync_dir";
                vector<FileInfo> files = FileInfo::list_files(path);
                FileInfo::print_list_files(files);
            }
            else if (cmd.rfind("exit", 0) == 0)
            {
                FileInfo::send_cmd("exit", sock);
                client.end_connection();
                return 0;
            }
            else
            {
                std::cerr << "Comando inválido." << std::endl;
            }
        }

        send_sync_thread.join();
        close(sock);

        return 0;
    }
}
