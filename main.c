


int main(int argc, char const *argv[]){
    /*readConfigFile("etc/config.txt");
    for(int i=0;i<7;i++)
        printf("%d-> %d\n", i, config[i]);
    */




    char* cache[]={"encrypt", "decrypt"};
    redirecionar("test_files/pdfTest.pdf", "test_files/novoPdfTest.pdf");
    aplicarTransformacoes(cache, 2);
    return 0;
}
