# JxServer

## JX Server -- a concurrency server application based on TCP Socket

- Using C to implement a server that able to handle thousands of request at the same time. The client's request include transit the specify file's path of the content or file informaiton such as the size of file and check the file's name exist or not in the given directory. 
- To improve the performance of handling multiple request, using epoll() to detect current connect file description has event or not.
- File transmission speed optimization. Using huffman's algorithm to build up huffman's tree and compress dictionary. That's used to achieve the file lossless compression and decompression.
- Using bitwise operation, such as bitGet and bitSet. That convenienct for implementing compress dictionary and improve the code readability for the compression logic.
- Involed techniques: C, TCP Socket, epoll(), huffman's algorithm and high concurrency.
