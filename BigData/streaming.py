import pyspark
from pyspark import *
from pyspark.conf import *
from pyspark.sql import *

from pyspark.ml.classification import RandomForestClassifier

from pyspark.ml import Pipeline
from pyspark.ml.classification import LogisticRegression
from pyspark.ml.evaluation import MulticlassClassificationEvaluator
from pyspark.ml.classification import GBTClassifier
from pyspark.ml.feature import StringIndexer, VectorIndexer

from pyspark.ml.feature import *
from pyspark.ml.feature import StopWordsRemover
from pyspark.ml.classification import DecisionTreeClassifier
import pprint


import os

os.environ['PYSPARK_SUBMIT_ARGS'] = '--packages org.apache.spark:spark-streaming-kafka-0-8_2.11:2.0.2 pyspark-shell'

import json
userdata_path = 'data3.json'
app_name = 'Userdata and their Friends'
master = 'local'

#Spark Configuration
spark = SparkConf().setAppName(app_name).setMaster(master).set('spark.executor.memory', '4G').set('spark.driver.memory', '45G').set('spark.driver.maxResultSize', '10G')
sc = SparkContext(conf=spark)
sqlContext = SQLContext (sc)
path = "data3.json"
peopleDF = sqlContext.read.json(path)

lis = []
with open('data3.json') as f:
    data = json.load(f)

lis = []
for elt in data:
    spli = elt.split("||")
    lis.append([int(spli[0]), spli[1]])



from pyspark.sql.types import *
cSchema = StructType([StructField("label",IntegerType()), StructField("text", StringType())])
train_list = lis
df = sqlContext.createDataFrame(train_list, schema=cSchema)


tokenizer = Tokenizer(inputCol="text", outputCol="words")
dfto = tokenizer.transform(df)
remover = StopWordsRemover(inputCol="words", outputCol="filtered")
dfre = remover.transform(dfto)
hashingTF = HashingTF(inputCol="filtered", outputCol="opfeatures")
dfha = hashingTF.transform(dfre)
idf = IDF(inputCol="opfeatures", outputCol="features")
dfid = idf.fit(dfha).transform(dfha)

# count_vectors = CountVectorizer(inputCol="filtered", outputCol="features", vocabSize=10000, minDF=5)
# dfid = count_vectors.fit(dfre).transform(dfre)


featureIndexer = VectorIndexer(inputCol="features", outputCol="indexedFeatures", maxCategories=32).fit(dfid)


from pyspark.ml import Pipeline
from pyspark.ml.classification import GBTClassifier
from pyspark.ml.feature import StringIndexer, VectorIndexer
from pyspark.ml.evaluation import MulticlassClassificationEvaluator
from pyspark.ml.classification import LogisticRegression, OneVsRest
from pyspark.ml.classification import MultilayerPerceptronClassifier
from pyspark.ml.classification import NaiveBayes



#(trainingData, testData) = dfid.randomSplit([0.80, 0.20], seed = 100)

#lr = LogisticRegression(maxIter=10, tol=1E-6, fitIntercept=True, labelCol="label", featuresCol="indexedFeatures",)

#ovr = OneVsRest(classifier=lr)

rf = RandomForestClassifier(labelCol="label", featuresCol="indexedFeatures", numTrees=10)

#lr= LogisticRegression(maxIter=100, regParam=0.01)

#dt=DecisionTreeClassifier(labelCol="label", featuresCol="indexedFeatures")
#pipelineOvr = Pipeline(stages=[featureIndexer,ovr])
#lr = LogisticRegression(maxIter=10, regParam=0.001)

pipelineRf = Pipeline(stages=[featureIndexer,rf])
#pipelineLR= Pipeline(stages=[featureIndexer,lr])


# compute accuracy on the test set
from pyspark.ml.evaluation import MulticlassClassificationEvaluator





##########################TRAINING PART Over###############


#featureIndexer = VectorIndexer(inputCol="features", outputCol="indexedFeatures", maxCategories=32).fit(dftsid)







import sys
from pyspark import SparkContext
from pyspark.streaming import *
from pyspark.streaming.kafka import *
from kafka import KafkaConsumer


sc = SparkContext.getOrCreate()
consumer = KafkaConsumer('guardian2', bootstrap_servers=['localhost:9092'], auto_offset_reset='earliest', consumer_timeout_ms=1000, value_deserializer=lambda x: x.decode('utf-8'))
lis = []
for con in consumer:
    temp = []
    temp.append(int(con.value.split('||')[0]))
    temp.append(con.value.split('||')[1])
    lis.append(temp)

context = SQLContext(sc)
testdata = context.createDataFrame(lis, schema=['label', 'text'])



testdatatokenizer = Tokenizer(inputCol="text", outputCol="words")
testdatato = testdatatokenizer.transform(testdata)
testdataremover = StopWordsRemover(inputCol="words", outputCol="filtered")
testdatadfre = testdataremover.transform(dfto)
hashingTF = HashingTF(inputCol="filtered", outputCol="opfeatures")
testdatadfha = hashingTF.transform(testdatadfre)
testdataidf = IDF(inputCol="opfeatures", outputCol="features")
testdata = testdataidf.fit(testdatadfha).transform(testdatadfha)





#pipelineFitOvr = pipelineOvr.fit(dfid)


#predictionsOvr = pipelineFitOvr.transform(testdata)

pipelineFitRf = pipelineRf.fit(dfid)
predictionsRf = pipelineFitRf.transform(testdata)

#pipelineFitLR = pipelineLR.fit(dfid)
#predictionsLR = pipelineFitLR.transform(testdata)




evaluatorAcc = MulticlassClassificationEvaluator(
      labelCol="label", predictionCol="prediction", metricName="accuracy")

evaluatorWP = MulticlassClassificationEvaluator(
      labelCol="label", predictionCol="prediction", metricName="weightedPrecision")

evaluatorWR = MulticlassClassificationEvaluator(
      labelCol="label", predictionCol="prediction", metricName="weightedRecall")



accuracyRf = evaluatorAcc.evaluate(predictionsRf)
print("Test Error for RF= %g" % (1.0 - accuracyRf))

WPRf = evaluatorWP.evaluate(predictionsRf)
print("Precision for RF = %g" % (WPRf))

WRRf = evaluatorWR.evaluate(predictionsRf)
print("Recall for RF = %g" % (WRRf))



# accuracyLR = evaluatorAcc.evaluate(predictionsLR)
# print("Test Error for Ovr= %g" % (1.0 - accuracyLR))
#
# WPLR = evaluatorWP.evaluate(predictionsLR)
# print("Precision for Ovr = %g" % (WPLR))
#
# WRLR = evaluatorWR.evaluate(predictionsLR)
# print("Recall for Ovr = %g" % (WRLR))
