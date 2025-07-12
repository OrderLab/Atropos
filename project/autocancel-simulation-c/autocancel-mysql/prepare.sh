./compile.sh
echo "PATH=/data/zeyin/autocancel-mysql/dist/bin:/data/zeyin/autocancel-mysql/dist/lib:$PATH" >> $HOME/.bashrc
./dist/scripts/mysql_install_db --user=mysql --basedir=/data/zeyin/autocancel-mysql/dist --datadir=/data/zeyin/autocancel-mysql/dist/data