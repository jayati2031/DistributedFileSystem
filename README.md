# Distributed File System using Sockets

## Overview

This project implements a **distributed file system** using **socket programming**. The system is composed of three servers: `Smain`, `Spdf`, and `Stext`, and supports multiple client connections. Clients can upload, download, and manage `.c`, `.pdf`, and `.txt` files. While clients interact only with `Smain`, the main server distributes files to `Spdf` (for `.pdf` files) and `Stext` (for `.txt` files) without the client's knowledge.

## Project Structure

The project consists of the following main components:

- **`Smain.c`**: The main server, responsible for handling client requests and routing files to the appropriate servers.
- **`Spdf.c`**: Server that manages and stores `.pdf` files.
- **`Stext.c`**: Server that manages and stores `.txt` files.
- **`client24s.c`**: Client program used to interact with `Smain` by sending commands for file operations.

## Server Details

### Smain

- Manages client connections.
- Stores `.c` files locally in the `~/smain` directory.
- Transfers `.pdf` files to `Spdf` and `.txt` files to `Stext`.
- Clients are unaware of `Spdf` and `Stext` and interact solely with `Smain`.

### Spdf & Stext

- **Spdf**: Stores `.pdf` files in the `~/spdf` directory and responds to requests from `Smain`.
- **Stext**: Stores `.txt` files in the `~/stext` directory and responds to requests from `Smain`.

## Client Commands

The client communicates with `Smain` by issuing the following commands:

1. **`ufile <filename> <destination_path>`**: 
   - Uploads a file to `Smain`.
   - The file is stored locally in `Smain` if it is a `.c` file, or it is routed to `Spdf` (`.pdf` files) or `Stext` (`.txt` files).
   
   **Examples**:
   ```bash
   client24s$ ufile sample.c ~smain/folder1/folder2
   client24s$ ufile sample.pdf ~smain/folder1/folder2

2. **`dfile <filename>`**: 
   - Downloads the specified file from `Smain` to the client's current working directory.
   - Depending on the file type:
     - If the file is a `.c` file, `Smain` processes the request locally and sends the file directly to the client.
     - If the file is a `.txt` file, `Smain` fetches it from `Stext` and then sends it to the client.
     - If the file is a `.pdf` file, `Smain` fetches it from `Spdf` and then sends it to the client.
   
   **Examples**:
   ```bash
   client24s$ dfile ~smain/folder1/folder2/sample.c
   client24s$ dfile ~smain/folder1/folder2/sample.txt
   client24s$ dfile ~smain/folder1/folder2/sample.pdf

3. **`rmfile <filename>`**: 
   - Removes (deletes) the specified file from `Smain`.
   - Depending on the file type:
     - If the file is a `.c` file, `Smain` deletes the file locally.
     - If the file is a `.txt` file, `Smain` sends a request to `Stext` to delete the file.
     - If the file is a `.pdf` file, `Smain` sends a request to `Spdf` to delete the file.

   **Example**:
   ```bash
   client24s$ rmfile ~smain/folder1/folder2/sample.c
   client24s$ rmfile ~smain/folder1/folder2/sample.txt
   client24s$ rmfile ~smain/folder1/folder2/sample.pdf

4. **`dtar <filetype>`**: 
   - Creates a tar file of all files of the specified type (`.c`, `.pdf`, `.txt`) and sends it to the client.
   - Depending on the file type:
     - If the file type is `.c`, `Smain` creates a tar file (`cfiles.tar`) of all `.c` files stored locally in `~/smain` and sends it to the client.
     - If the file type is `.pdf`, `Smain` requests `Spdf` to create a tar file (`pdf.tar`) of all `.pdf` files in `~/spdf`, and then sends the tar file to the client.
     - If the file type is `.txt`, `Smain` requests `Stext` to create a tar file (`text.tar`) of all `.txt` files in `~/stext`, and then sends the tar file to the client.

   **Examples**:
   ```bash
   client24s$ dtar .c
   client24s$ dtar .pdf
   client24s$ dtar .txt

4. **`display <pathname>`**: 
   - Displays a list of all .c, .pdf, and .txt files within the specified directory in Smain.
   - Smain retrieves the list of .pdf files from Spdf and .txt files from Stext, and combines them with the .c files in the given directory.
   - The consolidated list is sent to the client.
   - Only the filenames are displayed.

   **Examples**:
   ```bash
   client24s$ display ~smain/folder1/folder2

## Key Features

- **Multiple Client Support**: 
  - `Smain` can handle multiple clients simultaneously by forking new processes, enabling concurrent file operations without blocking other clients.

- **Transparent File Distribution**: 
  - Clients interact exclusively with `Smain`, which abstracts the underlying architecture. Clients are unaware that their files are actually distributed across `Spdf` and `Stext`.

- **Socket Communication**: 
  - All communication between the client and the servers is conducted via sockets, providing a reliable and efficient method for transferring data across the distributed system.

- **Directory Structure Management**: 
  - The system automatically manages the directory structure for file storage, ensuring that files are stored in their respective locations without client intervention.

- **Error Handling**: 
  - The system includes error handling mechanisms to manage various scenarios, such as invalid commands or file not found errors, enhancing user experience and reliability.

