./compile.sh
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export AUTOCANCEL_MYSQL_HOME="$SCRIPT_DIR"
echo "PATH=$AUTOCANCEL_MYSQL_HOME/dist/bin:$AUTOCANCEL_MYSQL_HOME/dist/lib:$PATH" >> $HOME/.bashrc
./dist/scripts/mysql_install_db --user=mysql --basedir=$AUTOCANCEL_MYSQL_HOME/dist --datadir=$AUTOCANCEL_MYSQL_HOME/dist/data