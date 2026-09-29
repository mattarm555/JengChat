Place the public server.crt generated on your Ubuntu VM in this folder.
The client checks this certificate and the configured server IP before sending credentials.
Do not place server.key or any account database in the client assets.
The Windows/macOS packages automatically include assets/security/server.crt.
